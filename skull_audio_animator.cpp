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
      m_currentSkit(ParsedSkit()), m_currentSkitLineNumber(0), m_isCurrentlySpeaking(false),
      FFT(vReal, vImag, SAMPLES, SAMPLE_RATE)
{
}

void SkullAudioAnimator::begin()
{
    m_audioPlayer.begin();
}

void SkullAudioAnimator::update()
{
    m_audioPlayer.update();
    updateSkit();
    updateEyes();
    //CAMKILL: removing for now as it's endlessly calling provideAudioFrames and burning the data
    //updateJawPosition();
}


    // ** CLOSE! It's setting eyes back to 255 after I set them to 100 in setup. Why?

// Determine the currently playing skit and line.
// Will set m_currentSkit and m_currentSkitLineNumber to the current skit and line.
void SkullAudioAnimator::updateSkit()
{
    if (!m_audioPlayer.isAudioPlaying())
    {
        //Serial.println("SkullAudioAnimator: m_audioPlayer.isCurrentlyPlaying()=false so m_isCurrentlySpeaking=false");
        m_isCurrentlySpeaking = false;
        return;
    }

    const String filePath = m_audioPlayer.getCurrentlyPlayingFilePath();
    if (filePath.isEmpty())
    {
        m_currentAudioFilePath = filePath;
        //Serial.println("SkullAudioAnimator: filePath.isEmpty() so m_isCurrentlySpeaking=false");
        m_isCurrentlySpeaking = false;
        return;
    }

    // If the file path is different from the current skit path, we're playing a new skit.
    if (filePath != m_currentAudioFilePath)
    {
        m_currentAudioFilePath = filePath;

        // If there no no associated skit, it's assumed to be something this skull should say.
        m_currentSkit = findSkitByName(m_skits, filePath);
        if (m_currentSkit.lines.empty())
        {
            //Serial.printf("SkullAudioAnimator: No associated skit found for %s\n", filePath.c_str());
            //Serial.println("SkullAudioAnimator: No associated skit so m_isCurrentlySpeaking=true");
            m_isCurrentlySpeaking = true;

            return;
        }

        //Serial.printf("SkullAudioAnimator: Playing new skit: %s\n", m_currentSkit.audioFile.c_str());

        // Keep only the lines that are for us.
        std::vector<ParsedSkitLine> lines;
        for (const auto &line : m_currentSkit.lines)
        {
            if ((line.speaker == 'A' && m_isPrimary) || (line.speaker == 'B' && !m_isPrimary))
            {
                lines.push_back(line);
            }
        }
        //Serial.printf("SkullAudioAnimator: Parsed skit '%s' with %d lines. %d lines for us.\n", m_currentSkit.audioFile.c_str(), m_currentSkit.lines.size(), lines.size());
        m_currentSkit.lines = lines;
    }

    // We're playing a skit. Is it our line?
    // The lines have been filtered to only include the lines for us.
    // We need to check if the current time is after the line's timestamp and before the line's timestamp + duration.
    unsigned long playbackTime = m_audioPlayer.getPlaybackTime();
    for (const auto &line : m_currentSkit.lines)
    {
        if (playbackTime >= line.timestamp && playbackTime < line.timestamp + line.duration)
        {
            if (m_currentSkitLineNumber != line.lineNumber)
            {
                //Serial.printf("SkullAudioAnimator: Now speaking line %d\n", line.lineNumber);
            }
            m_currentSkitLineNumber = line.lineNumber;
            //Serial.println("SkullAudioAnimator: Speaking skit line so m_isCurrentlySpeaking=true");
            m_isCurrentlySpeaking = true;
            break;
        }
        else
        {
            m_currentSkitLineNumber = -1;
            //Serial.println("SkullAudioAnimator: Not speaking skit line so m_isCurrentlySpeaking=false");
            m_isCurrentlySpeaking = false;
        }
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
        //Serial.println("SkullAudioAnimator: m_isCurrentlySpeaking=true so Setting eye brightness to max");
        m_lightController.setEyeBrightness(LightController::BRIGHTNESS_MAX);
    }
    else
    {
        //Serial.println("SkullAudioAnimator: m_isCurrentlySpeaking=false so Setting eye brightness to dim");
        m_lightController.setEyeBrightness(LightController::BRIGHTNESS_DIM);
    }
}

void SkullAudioAnimator::playNow(const char *filePath)
{
    m_audioPlayer.playNow(filePath);
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

void SkullAudioAnimator::setBluetoothConnected(bool connected)
{
    m_audioPlayer.setBluetoothConnected(connected);
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

// CAMKILL: does this need to be exposed?
size_t SkullAudioAnimator::getTotalBytesRead() const
{
    return m_audioPlayer.getTotalBytesRead();
}

// CAMKILL: This is the one providing data. Despite audio_player.cpp having a provideAudioFrames
// which IS getting called.. somehow. But doesn't do anything.
// Regardless, perhaps we can use this one by saving the played buffer for use with FFT?
int32_t SkullAudioAnimator::provideAudioFrames(Frame *frame, int32_t frame_count)
{
    return m_audioPlayer.provideAudioFrames(frame, frame_count);
}

//CAMKILL: is this used? I thought all of this was done in the sd_card_manager.cpp file.
// Although.. where SHOULD it be? Probably here. This is the only thing that should know about skits.
ParsedSkit SkullAudioAnimator::parseSkitFile(const String &wavFile, const String &txtFile)
{
    // Implement skit file parsing logic
    ParsedSkit skit;
    skit.audioFile = wavFile;
    skit.txtFile = txtFile;
    return skit;
}

// CAMKILL: does this need to be exposed?
const ParsedSkit &SkullAudioAnimator::getCurrentSkit() const
{
    return m_currentSkit;
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