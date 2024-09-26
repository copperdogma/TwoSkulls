/*
    This file handles the animation of the skulls based on the audio.
    It uses the audio player to get the audio data and the servo controller to move the jaw.
    It also uses the light controller to control the eyes.

    Although it provides pass-throughs for playing audio, it has no effect on the playing state.
    It only reacts to what is being currently played, which is entirely controlled by the audio player.

    Note: Frame is defined in SoundData.h in https://github.com/pschatzmann/ESP32-A2DP like so:

      Frame(int ch1, int ch2){
        channel1 = ch1;
        channel2 = ch2;
      }
*/

#include "skull_audio_animator.h"
#include <cmath>
#include <algorithm>
#include <Arduino.h>

SkullAudioAnimator::SkullAudioAnimator(bool isPrimary, ServoController &servoController, LightController &lightController,
                                       std::vector<ParsedSkit> &skits, SDCardManager &sdCardManager, int servoMinDegrees, int servoMaxDegrees)
    : m_servoController(servoController),
      m_lightController(lightController),
      m_sdCardManager(sdCardManager),
      m_isPrimary(isPrimary),
      m_skits(skits),
      m_servoMinDegrees(servoMinDegrees),
      m_servoMaxDegrees(servoMaxDegrees),
      m_isCurrentlySpeaking(false),
      m_currentSkitLineNumber(0),
      m_wasAudioPlaying(false),
      FFT(vReal, vImag, SAMPLES, SAMPLE_RATE)
{
}

void SkullAudioAnimator::processAudioFrames(const Frame *frames, int32_t frameCount, const String &currentFile, unsigned long playbackTime)
{
    // Update internal state based on new audio data
    m_currentFile = currentFile;
    m_currentPlaybackTime = playbackTime;
    m_isAudioPlaying = (frameCount > 0);

    // Process audio frames for various animations
    updateSkit();
    updateEyes();
    performFFT(frames, frameCount);
    updateJawPosition(frames, frameCount);
}

void SkullAudioAnimator::updateJawPosition(const Frame *frames, int32_t frameCount)
{
    if (frameCount > 0)
    {
        // Find the maximum amplitude in the current frame set
        int16_t maxAmplitude = 0;
        for (int32_t i = 0; i < frameCount; ++i)
        {
            maxAmplitude = std::max(maxAmplitude, static_cast<int16_t>(std::abs(frames[i].channel1)));
            maxAmplitude = std::max(maxAmplitude, static_cast<int16_t>(std::abs(frames[i].channel2)));
        }

        // Map the amplitude to jaw position
        int jawPosition = map(maxAmplitude, 0, 32767, m_servoMinDegrees, m_servoMaxDegrees);
        m_servoController.setPosition(jawPosition);
    }
    else
    {
        // Close the jaw when there's no audio
        m_servoController.setPosition(m_servoMinDegrees);
    }
}

void SkullAudioAnimator::updateSkit()
{
    if (m_wasAudioPlaying && !m_isAudioPlaying)
    {
        Serial.printf("SkullAudioAnimator: Finished playing audio file: %s\n", m_currentAudioFilePath.c_str());
        m_currentAudioFilePath = "";
        m_currentSkit = ParsedSkit();
        m_currentSkitLineNumber = -1;
    }
    m_wasAudioPlaying = m_isAudioPlaying;

    if (!m_isAudioPlaying)
    {
        m_isCurrentlySpeaking = false;
        return;
    }

    if (m_currentFile.isEmpty())
    {
        Serial.println("SkullAudioAnimator: currentFile is empty; setting m_isCurrentlySpeaking to false");
        m_isCurrentlySpeaking = false;
        return;
    }

    if (m_currentFile != m_currentAudioFilePath)
    {
        m_currentAudioFilePath = m_currentFile;
        m_currentSkitLineNumber = -1;

        m_currentSkit = findSkitByName(m_skits, m_currentFile);
        if (m_currentSkit.lines.empty())
        {
            Serial.printf("SkullAudioAnimator: Playing non-skit audio file: %s\n", m_currentFile.c_str());
            m_isCurrentlySpeaking = true;
            return;
        }

        Serial.printf("SkullAudioAnimator: Playing new skit: %s\n", m_currentSkit.audioFile.c_str());

        std::vector<ParsedSkitLine> lines;
        for (const auto &line : m_currentSkit.lines)
        {
            if ((line.speaker == 'A' && m_isPrimary) || (line.speaker == 'B' && !m_isPrimary))
            {
                lines.push_back(line);
            }
        }
        Serial.printf("SkullAudioAnimator: Parsed skit '%s' with %d lines. %d lines for us.\n",
                      m_currentSkit.audioFile.c_str(), m_currentSkit.lines.size(), lines.size());
        m_currentSkit.lines = lines;
    }

    size_t originalLineNumber = m_currentSkitLineNumber;
    bool foundLine = false;
    for (const auto &line : m_currentSkit.lines)
    {
        if (m_currentPlaybackTime >= line.timestamp && m_currentPlaybackTime < line.timestamp + line.duration)
        {
            m_currentSkitLineNumber = line.lineNumber;
            foundLine = true;
            break;
        }
    }

    if (m_currentSkitLineNumber != originalLineNumber && !m_currentSkit.lines.empty())
    {
        Serial.printf("SkullAudioAnimator: Now speaking line %d\n", m_currentSkitLineNumber);
    }

    if (m_isCurrentlySpeaking && !foundLine && !m_currentSkit.lines.empty())
    {
        Serial.printf("SkullAudioAnimator: Ended speaking line %d\n", m_currentSkitLineNumber);
    }

    if (!m_currentSkit.lines.empty())
    {
        m_isCurrentlySpeaking = foundLine;
    }
    else
    {
        m_isCurrentlySpeaking = true;
    }
}

void SkullAudioAnimator::updateEyes()
{
    if (m_isCurrentlySpeaking)
    {
        m_lightController.setEyeBrightness(LightController::BRIGHTNESS_MAX);
    }
    else
    {
        m_lightController.setEyeBrightness(LightController::BRIGHTNESS_DIM);
    }
}

ParsedSkit SkullAudioAnimator::findSkitByName(const std::vector<ParsedSkit> &skits, const String &name)
{
    for (const auto &skit : skits)
    {
        if (skit.audioFile == name)
        {
            return skit;
        }
    }
    return ParsedSkit();
}

void SkullAudioAnimator::performFFT(const Frame *frames, int32_t frameCount)
{
    if (frameCount < SAMPLES)
        return;

    for (uint16_t i = 0; i < SAMPLES; i++)
    {
        vReal[i] = (double)frames[i].channel1;
        vImag[i] = 0;
    }

    FFT.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.Compute(FFT_FORWARD);
    FFT.ComplexToMagnitude();
}

double SkullAudioAnimator::getFFTResult(int index)
{
    if (index >= 0 && index < SAMPLES / 2)
    {
        return vReal[index];
    }
    return 0;
}

double SkullAudioAnimator::calculateRMS(const int16_t *samples, int numSamples)
{
    double sum = 0;
    for (int i = 0; i < numSamples; i++)
    {
        sum += samples[i] * samples[i];
    }
    return sqrt(sum / numSamples);
}

void SkullAudioAnimator::onPlaybackStart(const String &filePath)
{
    // Implementation if needed
}

void SkullAudioAnimator::onPlaybackEnd(const String &filePath)
{
    // Implementation if needed
}