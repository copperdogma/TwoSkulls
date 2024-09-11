#include "skull_audio_animator.h"
#include <cmath>

SkullAudioAnimator::SkullAudioAnimator(bool isPrimary, ServoController &servoController, LightController &lightController, std::vector<ParsedSkit> &skits)
    : m_audioPlayer(), m_servoController(servoController), m_lightController(lightController),
      m_isPrimary(isPrimary), m_skits(skits), m_isPlayingSkit(false), m_currentSkitPath(""),
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
    updateJawPosition();
}

// Determine the currently playing skit and line.
// Will set m_isPlayingSkit to true if it's playing a skit, and false if it's not.
// Will set m_currentSkit and m_currentSkitLineNumber to the current skit and line.
void SkullAudioAnimator::updateSkit()
{
    const String filePath = m_audioPlayer.getCurrentlyPlayingFilePath();
    if (filePath.isEmpty())
    {
        m_isPlayingSkit = false;
        return;
    }

    // NOTE: the audio it's playing MIGHT NOT BE A SKIT. Maybe rename this to "updateAudio()"

    // I think this code is too skit-centric.
    // Maybe it shouldn't be m_currentSkitPath, but m_currentAudioFilePath?
    // We already have an m_currentSkit, to track the current skit.

    // If the file path is different from the current skit path, we're playing a new skit.
    if (filePath != m_currentSkitPath)
    {
        m_currentSkit = findSkitByName(m_skits, filePath);
        if (m_currentSkit.lines.empty())
        {
            // Print out the skits
            Serial.println("SkullAudioAnimator: Skits:");
            for (const auto &skit : m_skits)
            {
                Serial.println(skit.audioFile);
            }
            Serial.printf("SkullAudioAnimator: WARNING: Skit '%s' not found or empty. Skipping.\n", filePath.c_str());
            m_isPlayingSkit = false;
            return;
        }

        Serial.printf("SkullAudioAnimator: Playing new skit: %s\n", m_currentSkit.audioFile.c_str());

        // Keep only the lines that are for us.
        std::vector<ParsedSkitLine> lines;
        for (const auto &line : m_currentSkit.lines)
        {
            if ((line.speaker == 'A' && m_isPrimary) || (line.speaker == 'B' && !m_isPrimary))
            {
                lines.push_back(line);
            }
        }
        Serial.printf("SkullAudioAnimator: Parsed skit '%s' with %d lines. %d lines for us.\n", m_currentSkit.audioFile.c_str(), m_currentSkit.lines.size(), lines.size());
        m_currentSkit.lines = lines;

        m_isPlayingSkit = true;
        m_currentSkitPath = filePath;
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
                Serial.printf("SkullAudioAnimator: Now speaking line %d\n", line.lineNumber);
            }
            m_currentSkitLineNumber = line.lineNumber;
            m_isCurrentlySpeaking = true;
            break;
        }
        else
        {
            m_currentSkitLineNumber = -1;
            m_isCurrentlySpeaking = false;
        }
    }
}

void SkullAudioAnimator::updateJawPosition()
{
    if (m_audioPlayer.isCurrentlyPlaying())
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

void SkullAudioAnimator::playNow(const char *filePath)
{
    // CAMKILL: why are we resetting the bluetooth connection state here?
    // Just because we're playing a new file doesn't mean we're disconnecting from bluetooth...
    m_audioPlayer.setBluetoothConnected(false); // Reset connection state
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

// CAMKILL: does this need to be exposed? NB: I think most fo them aren't. Check the .h file
// But are they used? Do they need their own function wrapper if they're only used internally?
bool SkullAudioAnimator::isCurrentlyPlaying()
{
    return m_audioPlayer.isCurrentlyPlaying();
}

// CAMKILL: does this need to be exposed?
bool SkullAudioAnimator::isPlayingSkit() const
{
    return m_isPlayingSkit;
}

// CAMKILL: does this need to be exposed?
bool SkullAudioAnimator::hasFinishedPlaying()
{
    return m_audioPlayer.hasFinishedPlaying();
}

// CAMKILL: does this need to be exposed?
bool SkullAudioAnimator::isCurrentlySpeaking() const
{
    return m_isCurrentlySpeaking;
}

// CAMKILL: does this need to be public?
void SkullAudioAnimator::setBluetoothConnected(bool connected)
{
    m_audioPlayer.setBluetoothConnected(connected);
}

//CAMKILL: what is this even for?
void SkullAudioAnimator::setAudioReadyToPlay(bool ready)
{
    m_audioPlayer.setAudioReadyToPlay(ready);
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

void SkullAudioAnimator::logState()
{
    Serial.println("SkullAudioAnimator State:");
    Serial.printf("  Is playing skit: %d\n", m_isPlayingSkit);
    Serial.printf("  Current skit line: %d\n", m_currentSkitLineNumber);
}

// CAMKILL: does this need to be here? Isn't all of this functionality already handled by the AudioPlayer??
int32_t SkullAudioAnimator::provideAudioFrames(Frame *frame, int32_t frame_count)
{
    if (!m_audioPlayer.isCurrentlyPlaying())
    {
        memset(frame, 0, frame_count * sizeof(Frame));
        return frame_count;
    }

    size_t bytesToRead = frame_count * sizeof(Frame);
    size_t bytesRead = m_audioPlayer.readAudioData((uint8_t *)frame, bytesToRead);

    if (bytesRead < bytesToRead)
    {
        memset((uint8_t *)frame + bytesRead, 0, bytesToRead - bytesRead);

        if (bytesRead == 0)
        {
            // Kept this log as it's useful for tracking playback status
            Serial.println("SkullAudioAnimator: End of file reached, attempting to play next file");
            m_audioPlayer.playNext(nullptr);
        }
    }

    m_audioPlayer.incrementTotalBytesRead(bytesRead);
    return frame_count;
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
            vReal[i] = (double)buffer[i].channel1; // Use channel1 or average of both channels
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