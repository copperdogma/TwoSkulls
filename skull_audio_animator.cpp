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
#include <cstdlib>

SkullAudioAnimator::SkullAudioAnimator(bool isPrimary, ServoController& servoController, LightController& lightController, 
    std::vector<ParsedSkit>& skits, SDCardManager& sdCardManager, int servoMinDegrees, int servoMaxDegrees)
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
    // Constructor body (if needed)
}

void SkullAudioAnimator::update()
{
    Serial.println("SkullAudioAnimator::update(): DOING NOTHING");
    return;

    // The audio player is updated when playNext() is called or when the audio engine needs more data,
    // so we don't need to worry about it here. Here we're just reacting to what the audio player is playing.
    updateSkit();
    updateEyes();
    // CAMKILL: removing for now as it's endlessly calling provideAudioFrames and burning the data
    // updateJawPosition();
}

// Determine the currently playing skit and line.
// Will set m_currentSkit and m_currentSkitLineNumber to the current skit and line.
void SkullAudioAnimator::updateSkit()
{
    // TODO: Implement proper audio playback detection
    //ORIGL: bool isAudioPlaying = m_audioPlayer.isAudioPlaying();
    bool isAudioPlaying = false; // Placeholder, replace with actual implementation later

    // Detect when audio playback has just stopped
    if (m_wasAudioPlaying && !isAudioPlaying)
    {
        Serial.printf("SkullAudioAnimator: Finished playing audio file: %s\n", m_currentAudioFilePath.c_str());
        m_currentAudioFilePath = "";  // Reset current audio file path after logging
        m_currentSkit = ParsedSkit(); // Reset current skit
        m_currentSkitLineNumber = -1; // Reset current skit line number
    }
    m_wasAudioPlaying = isAudioPlaying;

    if (!isAudioPlaying)
    {
        m_isCurrentlySpeaking = false;
        return;
    }

    // TODO: Implement getting current playing file path
    //ORIG: const String filePath = m_audioPlayer.getCurrentlyPlayingFilePath();
    const String filePath = ""; // Placeholder, replace with actual implementation later

    if (filePath.isEmpty())
    {
        Serial.println("SkullAudioAnimator: filePath.isEmpty(); setting m_isCurrentlySpeaking to false");
        m_isCurrentlySpeaking = false;
        return;
    }

    if (filePath != m_currentAudioFilePath)
    {
        m_currentAudioFilePath = filePath;
        m_currentSkitLineNumber = -1; // Reset line number when new audio starts

        m_currentSkit = findSkitByName(m_skits, filePath);
        if (m_currentSkit.lines.empty())
        {
            Serial.printf("SkullAudioAnimator: Playing non-skit audio file: %s\n", filePath.c_str());
            m_isCurrentlySpeaking = true;
            return;
        }

        Serial.printf("SkullAudioAnimator: Playing new skit: %s\n", m_currentSkit.audioFile.c_str());

        // Filter lines for the current skull
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

    // TODO: Implement getting playback time
    //ORIG: unsigned long playbackTime = m_audioPlayer.getPlaybackTime();
    unsigned long playbackTime = 0; // Placeholder, replace with actual implementation later

    size_t originalLineNumber = m_currentSkitLineNumber;
    bool foundLine = false;
    for (const auto &line : m_currentSkit.lines)
    {
        if (playbackTime >= line.timestamp && playbackTime < line.timestamp + line.duration)
        {
            m_currentSkitLineNumber = line.lineNumber;
            foundLine = true;
            break;
        }
    }

    // Log when starting a new line, only if there are lines
    if (m_currentSkitLineNumber != originalLineNumber && !m_currentSkit.lines.empty())
    {
        Serial.printf("SkullAudioAnimator: Now speaking line %d\n", m_currentSkitLineNumber);
    }

    // Log when ending a line, only if there are lines
    if (m_isCurrentlySpeaking && !foundLine && !m_currentSkit.lines.empty())
    {
        Serial.printf("SkullAudioAnimator: Ended speaking line %d\n", m_currentSkitLineNumber);
    }

    // Update speaking status
    if (!m_currentSkit.lines.empty())
    {
        m_isCurrentlySpeaking = foundLine;
    }
    else
    {
        // For non-skit audio files, keep m_isCurrentlySpeaking true while audio is playing
        m_isCurrentlySpeaking = true;
    }

    // TODO: Implement audio muting
    //ORIG:  m_audioPlayer.setMuted(!m_isCurrentlySpeaking);
}

void SkullAudioAnimator::updateJawPosition()
{
    // if (m_audioPlayer.hasRemainingAudioData())
    // {
    //     performFFT();
    //     double bassEnergy = 0;
    //     for (int i = 1; i < 10; i++)
    //     {
    //         bassEnergy += getFFTResult(i);
    //     }
    //     int jawPosition = map(bassEnergy, 0, 1000, 0, 90);
    //     m_servoController.setPosition(jawPosition);
    // }
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
    return ParsedSkit(); // Return an empty ParsedSkit if not found
}

void SkullAudioAnimator::performFFT()
{
    // Get the current audio buffer
    // Frame buffer[SAMPLES];
    // int32_t bufferSize = m_audioPlayer.provideAudioFrames(buffer, SAMPLES);

    // Fill vReal with audio samples
    // for (uint16_t i = 0; i < SAMPLES; i++)
    // {
    //     if (i < bufferSize)
    //     {
    //         vReal[i] = (double)buffer[i].channel1; // Use channel 1 or average of both channels
    //     }
    //     else
    //     {
    //         vReal[i] = 0;
    //     }
    //     vImag[i] = 0;
    // }

    // Perform FFT
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
    // ... existing code ...
}

void SkullAudioAnimator::onPlaybackEnd(const String &filePath)
{
    // ... existing code ...
}

void SkullAudioAnimator::processAudioFrames(const Frame* frames, int32_t frameCount, const String& currentFile, unsigned long playbackTime)
{
    m_currentFile = currentFile;
    m_currentPlaybackTime = playbackTime;
    m_isAudioPlaying = (frameCount > 0);

    // Basic jaw position calculation based on audio amplitude
    if (frameCount > 0)
    {
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
        // If no frames, close the jaw
        m_servoController.setPosition(m_servoMinDegrees);
    }

    // Update skit information
    updateSkit();

    // Update eyes based on speaking state
    updateEyes();
}