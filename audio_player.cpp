#include "audio_player.h"
#include "sd_card_manager.h"
#include <cmath>
#include <algorithm>
#include <Arduino.h>
#include <esp_task_wdt.h>
#include <mutex>
#include <thread>

// TODO:
// - The light just stays on between files when it's speaking.
// Let's add a 200ms after we're finished playing a file before starting hte next one.

AudioPlayer::AudioPlayer(SDCardManager* sdCardManager)
    : m_totalBytesRead(0), m_writePos(0), m_readPos(0), m_bufferFilled(0),
      m_currentFilePath(""), m_isAudioPlaying(false), m_muted(false),
      m_currentPlaybackTime(0), m_lastFrameTime(0), m_sdCardManager(sdCardManager)
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

    while (bytesRead < bytesToRead && m_bufferFilled > 0)
    {
        size_t chunkSize = std::min(bytesToRead - bytesRead, m_bufferFilled);
        size_t readPos = m_readPos % AUDIO_BUFFER_SIZE;

        // Handle buffer wrap-around
        size_t firstChunkSize = std::min(chunkSize, AUDIO_BUFFER_SIZE - readPos);
        memcpy((uint8_t *)frame + bytesRead, m_audioBuffer + readPos, firstChunkSize);

        m_readPos = (readPos + firstChunkSize) % AUDIO_BUFFER_SIZE;
        m_bufferFilled -= firstChunkSize;
        bytesRead += firstChunkSize;

        // If there is more data to read and the buffer wraps around
        if (firstChunkSize < chunkSize)
        {
            size_t secondChunkSize = chunkSize - firstChunkSize;
            memcpy((uint8_t *)frame + bytesRead, m_audioBuffer, secondChunkSize);

            m_readPos = secondChunkSize;
            m_bufferFilled -= secondChunkSize;
            bytesRead += secondChunkSize;
        }
    }

    // Zero-fill any remaining frames if we have insufficient data
    if (bytesRead < bytesToRead)
    {
        memset((uint8_t *)frame + bytesRead, 0, bytesToRead - bytesRead);
    }

    m_totalBytesRead += bytesRead;

    fillBuffer(); // Fill the buffer if needed

    // Determine if audio is still playing
    if (bytesRead == 0 && m_bufferFilled == 0 && (!audioFile || !audioFile.available()) && audioQueue.empty())
    {
        m_isAudioPlaying = false;
    }
    else
    {
        m_isAudioPlaying = true;
    }

    if (m_muted)
    {
        // Zero out the frames to mute the audio
        memset(frame, 0, frame_count * sizeof(Frame));
    }
    
    // Update playback time based on elapsed time
    unsigned long now = millis();
    if (m_lastFrameTime != 0)
    {
        unsigned long elapsedTime = now - m_lastFrameTime;
        m_currentPlaybackTime += elapsedTime;
        m_lastFrameTime = now;
    }
    else
    {
        m_lastFrameTime = now;
    }

    return frame_count;
}

size_t AudioPlayer::readAudioDataFromFile(uint8_t *buffer, size_t bytesToRead)
{
    if (!audioFile || !audioFile.available())
    {
        String currentFilePath = audioFile.name();
        audioFile = m_sdCardManager->openFile(currentFilePath.c_str());
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

void AudioPlayer::fillBuffer()
{
    while (m_bufferFilled < AUDIO_BUFFER_SIZE)
    {
        if (!audioFile || !audioFile.available())
        {
            // Only start the next file if the buffer is empty
            if (m_bufferFilled == 0)
            {
                if (!startNextFile())
                {
                    break; // No more files to play
                }
                else
                {
                    // Reset playback time since we're starting a new file
                    m_currentPlaybackTime = 0;
                    m_lastFrameTime = millis();
                }
            }
            else
            {
                // Current file has ended, but there's still data in the buffer
                // Do not start the next file until buffer is empty
                break;
            }
        }

        // Proceed to fill the buffer if we have an open file
        if (audioFile && audioFile.available())
        {
            size_t spaceAvailable = AUDIO_BUFFER_SIZE - m_bufferFilled;
            size_t writePos = (m_readPos + m_bufferFilled) % AUDIO_BUFFER_SIZE;

            // Determine how much data to read
            size_t bytesToRead = std::min(spaceAvailable, (size_t)audioFile.available());

            // Handle buffer wrap-around
            size_t firstChunkSize = std::min(bytesToRead, AUDIO_BUFFER_SIZE - writePos);
            size_t bytesRead = audioFile.read(m_audioBuffer + writePos, firstChunkSize);

            if (bytesRead > 0)
            {
                m_bufferFilled += bytesRead;
                m_writePos = (writePos + bytesRead) % AUDIO_BUFFER_SIZE;

                // If there is more data to read and the buffer wraps around
                if (bytesRead < bytesToRead)
                {
                    size_t secondChunkSize = bytesToRead - bytesRead;
                    size_t additionalBytesRead = audioFile.read(m_audioBuffer, secondChunkSize);

                    if (additionalBytesRead > 0)
                    {
                        m_bufferFilled += additionalBytesRead;
                        m_writePos = additionalBytesRead;
                        bytesRead += additionalBytesRead;
                    }
                }
            }

            if (!audioFile.available())
            {
                audioFile.close();
            }
        }
        else
        {
            // No file to read from and buffer is not empty; exit loop
            break;
        }
    }
}

// CAMKILL might go away when skull_audio_animator updateJawPosition() is rewritten
bool AudioPlayer::hasRemainingAudioData()
{
    return audioFile && audioFile.available();
}

bool AudioPlayer::isAudioPlaying() const
{
    return m_isAudioPlaying;
}

unsigned long AudioPlayer::getPlaybackTime() const
{
    if (!m_isAudioPlaying)
    {
        return 0;
    }

    return m_currentPlaybackTime;
}

String AudioPlayer::getCurrentlyPlayingFilePath() const
{
    return m_currentFilePath;
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
        // Do not reset m_currentFilePath here; keep it for logging purposes
        return false;
    }

    std::string nextFile = audioQueue.front();
    audioQueue.pop();

    audioFile = m_sdCardManager->openFile(nextFile.c_str());
    if (!audioFile)
    {
        Serial.printf("Failed to open audio file: %s\n", nextFile.c_str());
        return startNextFile(); // Try the next file in the queue
    }

    m_currentFilePath = String(nextFile.c_str());
    m_isAudioPlaying = true;

    // Reset playback time since we're starting a new file
    m_currentPlaybackTime = 0;
    m_lastFrameTime = millis();

    return true;
}

void AudioPlayer::setMuted(bool muted)
{
    m_muted = muted;
}