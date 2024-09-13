/*
    This file handles the animation of the skulls based on the audio.
    It uses the audio player to get the audio data and the servo controller to move the jaw.
    It also uses the light controller to control the eyes.

    Although it provides pass-throughs for playing audio, it has no effect on the playing state.
    It only reacts to what is being currently played, which is entirely controlled by the audio player.
*/

#include "skull_audio_animator.h"
#include <cmath>

SkullAudioAnimator::SkullAudioAnimator(bool isPrimary, ServoController &servoController, LightController &lightController, std::vector<ParsedSkit> &skits)
    : m_audioPlayer(), m_servoController(servoController), m_lightController(lightController),
      m_isPrimary(isPrimary), m_skits(skits), m_currentAudioFilePath(""),
      m_currentSkit(ParsedSkit()), m_currentSkitLineNumber(-1), m_isCurrentlySpeaking(false),
      m_wasAudioPlaying(false), FFT(vReal, vImag, SAMPLES, SAMPLE_RATE)
{
}


void SkullAudioAnimator::begin()
{
    m_audioPlayer.begin();
}

void SkullAudioAnimator::update()
{
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
    bool isAudioPlaying = m_audioPlayer.isAudioPlaying();

    // Detect when audio playback has just stopped
    if (m_wasAudioPlaying && !isAudioPlaying)
    {
        Serial.printf("SkullAudioAnimator: Finished playing audio file: %s\n", m_currentAudioFilePath.c_str());
        m_currentAudioFilePath = "";          // Reset current audio file path after logging
        m_currentSkit = ParsedSkit();         // Reset current skit
        m_currentSkitLineNumber = -1;         // Reset current skit line number
    }
    m_wasAudioPlaying = isAudioPlaying;

    if (!isAudioPlaying)
    {
        if (m_isCurrentlySpeaking)
        {
            Serial.println("SkullAudioAnimator: No audio playing; setting m_isCurrentlySpeaking to false");
        }
        m_isCurrentlySpeaking = false;
        return;
    }

    const String filePath = m_audioPlayer.getCurrentlyPlayingFilePath();

    if (filePath.isEmpty())
    {
        Serial.println("SkullAudioAnimator: filePath.isEmpty(); setting m_isCurrentlySpeaking to false");
        // Do not reset m_currentAudioFilePath here
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
            Serial.println("SkullAudioAnimator: Playing non-skit audio file; Setting m_isCurrentlySpeaking to true");
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

    unsigned long playbackTime = m_audioPlayer.getPlaybackTime();
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
}

void SkullAudioAnimator::updateJawPosition()
{
    if (m_audioPlayer.hasRemainingAudioData())
    {
        performFFT();
        double bassEnergy = 0;
        for (int i = 1; i < 10; i++)
        {
            bassEnergy += getFFTResult(i);
        }
        int jawPosition = map(bassEnergy, 0, 1000, 0, 90);
        m_servoController.setPosition(jawPosition);
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

void SkullAudioAnimator::playNext(const char *filePath)
{
    Serial.printf("SkullAudioAnimator: Queuing next audio file: %s\n", filePath);
    m_audioPlayer.playNext(filePath);
}

void SkullAudioAnimator::playSkitNext(const ParsedSkit &skit)
{
    m_audioPlayer.playNext(skit.audioFile.c_str());
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

// CAMKILL: Perhaps we can use this one by saving the played buffer for use with FFT?
int32_t SkullAudioAnimator::provideAudioFrames(Frame *frame, int32_t frame_count)
{
    return m_audioPlayer.provideAudioFrames(frame, frame_count);
}

void SkullAudioAnimator::performFFT()
{
    // Get the current audio buffer
    Frame buffer[SAMPLES];
    int32_t bufferSize = m_audioPlayer.provideAudioFrames(buffer, SAMPLES);

    // Fill vReal with audio samples
    for (uint16_t i = 0; i < SAMPLES; i++)
    {
        if (i < bufferSize)
        {
            vReal[i] = (double)buffer[i].channel1; // Use channel 1 or average of both channels
        }
        else
        {
            vReal[i] = 0;
        }
        vImag[i] = 0;
    }

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