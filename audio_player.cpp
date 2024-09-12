#include "audio_player.h"
#include "file_manager.h"
#include <cmath>
#include <algorithm>
#include <Arduino.h>
#include <esp_task_wdt.h>
#include <mutex>
#include <thread>

// ** ISSUES:
// - it decides it's finished playing too early on the init.wav so it never turns on the lights
// - can we kill isPlaying? Just replace it with m_isAudioPlaying?


// TODO:
// Should update millis based on when the last frame was read.
// We need to keep track of this because we don't want to update the millis
// when we're not actually playing audio.
// We can do this by keeping a running total of the bytes read, and then
// using the bitrate of the audio to calculate the total playtime.
// Then we can subtract the current position in the file from the total playtime
// to get the current playtime.

// But we need a way to reset the millis back to zero when we start a new file.
// We can do this by checking if the current file is the same as the last file.
// If it is, then we reset the millis to zero.
// Otherwise, we keep the millis running.

AudioPlayer::AudioPlayer()
    : m_totalBytesRead(0), m_isBluetoothConnected(false),
      m_writePos(0), m_readPos(0), m_bufferFilled(0), m_currentFilePath(""),
      m_isAudioPlaying(false), m_mutex()
{
}

void AudioPlayer::begin()
{
}

void AudioPlayer::playNext(const char *filePath)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (filePath)
    {
        audioQueue.push(std::string(filePath));

        fillBuffer();
    }
}

// This function is called by the audio engine to get the next chunk of audio data.
// It it meant to only be called by the bluetooth speaker when it's actively playing audio.
// And that seems to be how it works. It doesn't get called at all until the bluetooth speaker is connected.
int32_t AudioPlayer::provideAudioFrames(Frame *frame, int32_t frame_count)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t bytesToRead = frame_count * sizeof(Frame);
    size_t bytesRead = 0;

    if (m_bufferFilled > 0)
    {
        bytesRead = std::min(m_bufferFilled, bytesToRead);
        memcpy(frame, m_audioBuffer + m_readPos, bytesRead);
        m_readPos = (m_readPos + bytesRead) % AUDIO_BUFFER_SIZE;
        m_bufferFilled -= bytesRead;
    }

    if (bytesRead < bytesToRead)
    {
        memset((uint8_t *)frame + bytesRead, 0, bytesToRead - bytesRead);
    }

    m_totalBytesRead += bytesRead; // Not actuall used for anything.
    m_isAudioPlaying = (bytesRead > 0);

    fillBuffer();

    return frame_count;
}

size_t AudioPlayer::readAudioDataFromFile(uint8_t *buffer, size_t bytesToRead)
{
    if (!audioFile || !audioFile.available())
    {
        String currentFilePath = audioFile.name();
        audioFile = SD.open(currentFilePath.c_str());
        if (!audioFile)
        {
            return 0;
        }
    }

    if (audioFile && audioFile.available())
    {
        size_t bytesRead = audioFile.read(buffer, bytesToRead);
        return bytesRead;
    }

    return 0;
}

void AudioPlayer::writeAudioDataToBuffer()
{
    if (!audioFile)
    {
        Serial.println("AudioPlayer: writeAudio called with null audioFile");
        return;
    }

    size_t spaceAvailable = AUDIO_BUFFER_SIZE - m_bufferFilled;
    size_t writeAmount = std::min(spaceAvailable, static_cast<size_t>(audioFile.available()));

    if (writeAmount == 0)
    {
        return;
    }

    if (m_writePos + writeAmount > AUDIO_BUFFER_SIZE)
    {
        size_t firstPart = AUDIO_BUFFER_SIZE - m_writePos;
        size_t secondPart = writeAmount - firstPart;
        size_t bytesRead1 = audioFile.read(m_audioBuffer + m_writePos, firstPart);
        size_t bytesRead2 = audioFile.read(m_audioBuffer, secondPart);
        m_writePos = secondPart;
        if (bytesRead1 != firstPart || bytesRead2 != secondPart)
        {
            Serial.println("AudioPlayer: Error reading file in split write");
        }
    }
    else
    {
        size_t bytesRead = audioFile.read(m_audioBuffer + m_writePos, writeAmount);
        m_writePos = (m_writePos + writeAmount) % AUDIO_BUFFER_SIZE;
        if (bytesRead != writeAmount)
        {
            Serial.println("AudioPlayer: Error reading file in single write");
        }
    }

    m_bufferFilled += writeAmount;
}

bool AudioPlayer::hasRemainingAudioData()
{
    return audioFile && audioFile.available();
}

bool AudioPlayer::shouldPlayAudio()
{
    return hasRemainingAudioData() && m_isBluetoothConnected;
}

bool AudioPlayer::isAudioPlaying() const
{
    return m_isAudioPlaying;
}

unsigned long AudioPlayer::getPlaybackTime() const
{
    // if (!isPlaying) {
    //     return 0;
    // }
    return millis() - m_currentFileStartTime;
}

String AudioPlayer::getCurrentlyPlayingFilePath() const
{
    return m_currentFilePath;
}

// If the buffer is not full, fill it.
// If it's reached the end of the file, try to start the next one.
// If there's no next file, we're done.
void AudioPlayer::fillBuffer()
{
    while (m_bufferFilled < AUDIO_BUFFER_SIZE)
    {
        if (!audioFile || !audioFile.available())
        {
            if (!startNextFile())
            {
                break;
            }
        }

        size_t spaceAvailable = AUDIO_BUFFER_SIZE - m_bufferFilled;
        size_t writeAmount = std::min(spaceAvailable, static_cast<size_t>(audioFile.available()));

        size_t writePos = (m_readPos + m_bufferFilled) % AUDIO_BUFFER_SIZE;
        size_t bytesRead = audioFile.read(m_audioBuffer + writePos, writeAmount);

        if (bytesRead != writeAmount)
        {
            Serial.println("Warning: Failed to read expected amount from file");
        }

        m_bufferFilled += bytesRead;

        if (bytesRead == 0)
        {
            // End of current file
            audioFile.close();
        }
    }
}

bool AudioPlayer::startNextFile()
{
    if (audioFile)
    {
        audioFile.close();
    }

    if (audioQueue.empty())
    {
        m_isAudioPlaying = false;
        m_currentFilePath = "";
        return false;
    }

    std::string nextFile = audioQueue.front();
    audioQueue.pop();

    audioFile = SD.open(nextFile.c_str());
    if (!audioFile)
    {
        Serial.printf("Failed to open audio file: %s\n", nextFile.c_str());
        return startNextFile(); // Try the next file in the queue
    }

    m_totalBytesRead = 0;
    m_currentFilePath = String(nextFile.c_str());  // Convert std::string to String
    m_isAudioPlaying = true;
    return true;
}