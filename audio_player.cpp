/*
    This is the audio player for the skull.

    Note: Frame is defined in SoundData.h in https://github.com/pschatzmann/ESP32-A2DP like so:

      Frame(int ch1, int ch2){
        channel1 = ch1;
        channel2 = ch2;
      }
*/

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

AudioPlayer::AudioPlayer(SDCardManager *sdCardManager)
    : m_totalBytesRead(0), m_writePos(0), m_readPos(0), m_bufferFilled(0),
      m_currentFilePath(""), m_isAudioPlaying(false), m_muted(false),
      m_currentPlaybackTime(0), m_lastFrameTime(0), m_sdCardManager(sdCardManager),
      m_playbackStartCallback(nullptr), m_playbackEndCallback(nullptr), m_audioFramesProvidedCallback(nullptr)
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
    size_t totalBytesRead = 0;

    while (bytesRead < bytesToRead && m_bufferFilled > 0)
    {
        size_t readPos = m_readPos % AUDIO_BUFFER_SIZE;

        if (m_expectFileIndex)
        {
            uint16_t fileIndex;
            if (m_bufferFilled >= sizeof(uint16_t))
            {
                if ((AUDIO_BUFFER_SIZE - readPos) >= sizeof(uint16_t))
                {
                    // Enough data to read file index without wrap-around
                    memcpy(&fileIndex, m_audioBuffer + readPos, sizeof(uint16_t));
                    m_readPos = (readPos + sizeof(uint16_t)) % AUDIO_BUFFER_SIZE;
                }
                else
                {
                    // Data wraps around
                    uint8_t temp[sizeof(uint16_t)];
                    size_t firstChunkSize = AUDIO_BUFFER_SIZE - readPos;
                    memcpy(temp, m_audioBuffer + readPos, firstChunkSize);
                    size_t secondChunkSize = sizeof(uint16_t) - firstChunkSize;
                    memcpy(temp + firstChunkSize, m_audioBuffer, secondChunkSize);
                    m_readPos = secondChunkSize;
                    memcpy(&fileIndex, temp, sizeof(uint16_t));
                }
                m_bufferFilled -= sizeof(uint16_t);

                // Update current file index and path
                m_currentFileIndex = fileIndex;
                m_currentFilePath = getFilePath(fileIndex);

                // Call playback start callback
                if (m_playbackStartCallback)
                {
                    m_playbackStartCallback(m_currentFilePath);
                }

                m_expectFileIndex = false;
            }
            else
            {
                // Not enough data to read the file index yet
                break;
            }
        }
        else
        {
            // Read audio data
            size_t chunkSize = std::min(bytesToRead - bytesRead, m_bufferFilled);
            size_t firstChunkSize = std::min(chunkSize, AUDIO_BUFFER_SIZE - readPos);
            memcpy((uint8_t *)frame + bytesRead, m_audioBuffer + readPos, firstChunkSize);

            m_readPos = (readPos + firstChunkSize) % AUDIO_BUFFER_SIZE;
            m_bufferFilled -= firstChunkSize;
            bytesRead += firstChunkSize;
            totalBytesRead += firstChunkSize;

            if (firstChunkSize < chunkSize)
            {
                size_t secondChunkSize = chunkSize - firstChunkSize;
                memcpy((uint8_t *)frame + bytesRead, m_audioBuffer, secondChunkSize);

                m_readPos = secondChunkSize;
                m_bufferFilled -= secondChunkSize;
                bytesRead += secondChunkSize;
                totalBytesRead += secondChunkSize;
            }

            // If buffer is empty, expect a new file index next time
            if (m_bufferFilled == 0 && (!audioFile || !audioFile.available()) && audioQueue.empty())
            {
                // Call playback end callback
                if (m_playbackEndCallback)
                {
                    m_playbackEndCallback(m_currentFilePath);
                }
                m_expectFileIndex = true;
            }
        }
    }

    // Zero-fill any remaining frames
    if (bytesRead < bytesToRead)
    {
        memset((uint8_t *)frame + bytesRead, 0, bytesToRead - bytesRead);
    }

    m_totalBytesRead += totalBytesRead;
    fillBuffer();

    // Update playback status and time
    m_isAudioPlaying = (bytesRead > 0 || m_bufferFilled > 0 || (audioFile && audioFile.available()) || !audioQueue.empty());

    if (m_muted)
    {
        memset(frame, 0, frame_count * sizeof(Frame));
    }

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

    // Call the frames provided callback if set
    if (m_audioFramesProvidedCallback)
    {
        m_audioFramesProvidedCallback(m_currentFilePath, frame, frame_count);
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

                    // Get the file index for the current file
                    uint16_t fileIndex = getFileIndex(m_currentFilePath);

                    // Write the file index into the buffer
                    size_t writePos = (m_readPos + m_bufferFilled) % AUDIO_BUFFER_SIZE;

                    if ((AUDIO_BUFFER_SIZE - m_bufferFilled) >= sizeof(uint16_t))
                    {
                        // Enough space to write the file index
                        memcpy(m_audioBuffer + writePos, &fileIndex, sizeof(uint16_t));
                        m_bufferFilled += sizeof(uint16_t);
                        m_writePos = (writePos + sizeof(uint16_t)) % AUDIO_BUFFER_SIZE;
                    }
                    else
                    {
                        // Buffer is full or wraps around, handle wrap-around
                        // First part
                        size_t firstPart = AUDIO_BUFFER_SIZE - writePos;
                        memcpy(m_audioBuffer + writePos, &fileIndex, firstPart);
                        // Second part
                        size_t secondPart = sizeof(uint16_t) - firstPart;
                        memcpy(m_audioBuffer, ((uint8_t*)&fileIndex) + firstPart, secondPart);
                        m_bufferFilled += sizeof(uint16_t);
                        m_writePos = secondPart;
                    }
                }
            }
            else
            {
                break;
            }
        }

        // Proceed to fill the buffer with audio data
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

        // Call the playback end callback if set
        if (m_playbackEndCallback)
        {
            m_playbackEndCallback(m_currentFilePath);
        }
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
        Serial.printf("AudioPlayer: Failed to open audio file: %s\n", nextFile.c_str());
        return startNextFile(); // Try the next file in the queue
    }

    // Skip WAV header (44 bytes)
    audioFile.seek(44);

    m_currentFilePath = String(nextFile.c_str());
    m_isAudioPlaying = true;

    // Reset playback time since we're starting a new file
    m_currentPlaybackTime = 0;
    m_lastFrameTime = millis();

    // Call the playback start callback if set
    if (m_playbackStartCallback)
    {
        m_playbackStartCallback(m_currentFilePath);
    }

    return true;
}

void AudioPlayer::setMuted(bool muted)
{
    m_muted = muted;
}

uint16_t AudioPlayer::getFileIndex(const String &filePath)
{
    auto it = std::find(m_fileList.begin(), m_fileList.end(), filePath);
    if (it != m_fileList.end())
    {
        return std::distance(m_fileList.begin(), it);
    }
    else
    {
        m_fileList.push_back(filePath);
        return m_fileList.size() - 1;
    }
}

String AudioPlayer::getFilePath(uint16_t fileIndex)
{
    if (fileIndex < m_fileList.size())
    {
        return m_fileList[fileIndex];
    }
    else
    {
        return "";
    }
}