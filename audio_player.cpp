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

AudioPlayer::AudioPlayer(SDCardManager *sdCardManager)
    : m_totalBytesRead(0), m_writePos(0), m_readPos(0), m_bufferFilled(0),
      m_currentFilePath(""), m_isAudioPlaying(false), m_muted(false),
      m_currentPlaybackTime(0), m_lastFrameTime(0), m_sdCardManager(sdCardManager),
      m_playbackStartCallback(nullptr), m_playbackEndCallback(nullptr), m_audioFramesProvidedCallback(nullptr),
      m_currentFileIndex(0)
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
        String path(filePath);
        uint16_t newFileIndex = addFileToList(path); // Add the file to the list and get the index
        audioQueue.push(std::string(filePath));
        Serial.printf("AudioPlayer: Added file (index: %d) to queue: %s\n", newFileIndex, filePath);
    }
}

int32_t AudioPlayer::provideAudioFrames(Frame *frame, int32_t frame_count)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t bytesToRead = frame_count * sizeof(Frame);
    size_t bytesRead = 0;

    while (bytesRead < bytesToRead && m_bufferFilled > 0)
    {
        // Read data from buffer with wrap-around handling
        size_t chunkSize = std::min(bytesToRead - bytesRead, m_bufferFilled);
        size_t firstChunkSize = std::min(chunkSize, AUDIO_BUFFER_SIZE - m_readPos);
        memcpy((uint8_t *)frame + bytesRead, m_audioBuffer + m_readPos, firstChunkSize);

        m_readPos = (m_readPos + firstChunkSize) % AUDIO_BUFFER_SIZE;
        m_bufferFilled -= firstChunkSize;
        bytesRead += firstChunkSize;

        if (firstChunkSize < chunkSize)
        {
            size_t secondChunkSize = chunkSize - firstChunkSize;
            memcpy((uint8_t *)frame + bytesRead, m_audioBuffer + m_readPos, secondChunkSize);

            m_readPos = (m_readPos + secondChunkSize) % AUDIO_BUFFER_SIZE;
            m_bufferFilled -= secondChunkSize;
            bytesRead += secondChunkSize;
        }
    }

    // Process the data read to handle file indices
    size_t processedBytes = 0;
    while (processedBytes < bytesRead)
    {
        // Read the file index
        uint16_t fileIndex;
        memcpy(&fileIndex, (uint8_t *)frame + processedBytes, sizeof(uint16_t));
        processedBytes += sizeof(uint16_t);

        if (fileIndex != SAME_FILE)
        {
            if (fileIndex == END_OF_FILE)
            {
                // Call playback end callback
                if (m_playbackEndCallback)
                {
                    m_playbackEndCallback(m_currentFilePath);
                }
                Serial.printf("AudioPlayer: Playback ended for file: %s\n", m_currentFilePath.c_str());
                m_currentFilePath = "";
            }
            else
            {
                // New file started
                m_currentFileIndex = fileIndex;
                m_currentFilePath = getFilePath(fileIndex);
                // Call playback start callback only if we have a valid file path
                if (m_playbackStartCallback && !m_currentFilePath.isEmpty())
                {
                    m_playbackStartCallback(m_currentFilePath);
                }
                // Serial.printf("AudioPlayer: Playback started for file: %s\n", m_currentFilePath.c_str());
            }
        }

        // Move the frame pointer past the file index
        size_t audioDataSize = sizeof(Frame) - sizeof(uint16_t);
        processedBytes += audioDataSize;
    }

    // Zero-fill any remaining frames
    if (bytesRead < bytesToRead)
    {
        memset((uint8_t *)frame + bytesRead, 0, bytesToRead - bytesRead);
    }

    m_totalBytesRead += bytesRead;
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

void AudioPlayer::fillBuffer()
{
    while (m_bufferFilled < AUDIO_BUFFER_SIZE)
    {
        uint16_t fileIndex = SAME_FILE;

        if (!audioFile || !audioFile.available())
        {
            if (audioFile)
            {
                // End of current file
                audioFile.close();
                audioFile = File();

                // Write END_OF_FILE to buffer
                writeToBuffer(END_OF_FILE, nullptr, 0);
            }

            // Start the next file
            if (!startNextFile())
            {
                // No more files to play
                break;
            }
            else
            {
                // New file started
                fileIndex = m_currentFileIndex;
            }
        }

        // Debugging:
        switch (fileIndex) {
            //case SAME_FILE: Serial.println("AudioPlayer:fillBuffer(): writing fileIndex SAME_FILE"); break;
            case SAME_FILE: break;
            case END_OF_FILE: Serial.println("AudioPlayer:fillBuffer(): writing fileIndex END_OF_FILE"); break;
            default: Serial.printf("AudioPlayer:fillBuffer(): starting new file: setting fileIndex to actively playing file index: %d\n", fileIndex); break;
        }

        if (audioFile && audioFile.available())
        {
            size_t spaceAvailable = AUDIO_BUFFER_SIZE - m_bufferFilled;
            const size_t MAX_READ_SIZE = 512; // Define a reasonable maximum read size
            size_t bytesToRead = std::min({spaceAvailable - sizeof(uint16_t), (size_t)audioFile.available(), MAX_READ_SIZE});

            uint8_t audioData[MAX_READ_SIZE]; // Fixed-size buffer
            size_t bytesRead = audioFile.read(audioData, bytesToRead);

            if (bytesRead > 0)
            {
                writeToBuffer(fileIndex, audioData, bytesRead);
                fileIndex = SAME_FILE; // Subsequent data is from the same file
            }
            else
            {
                // No data read; close the file
                audioFile.close();
                audioFile = File();

                // Write END_OF_FILE to buffer
                writeToBuffer(END_OF_FILE, nullptr, 0);
            }
        }
        else
        {
            break;
        }
    }
}

void AudioPlayer::writeToBuffer(uint16_t fileIndex, const uint8_t *audioData, size_t dataSize)
{
    // Create a combined buffer with fileIndex and audioData
    size_t totalSize = sizeof(uint16_t) + dataSize;
    uint8_t tempBuffer[sizeof(uint16_t) + dataSize];

    // Copy the file index
    memcpy(tempBuffer, &fileIndex, sizeof(uint16_t));

    // Copy the audio data if available
    if (audioData && dataSize > 0)
    {
        memcpy(tempBuffer + sizeof(uint16_t), audioData, dataSize);
    }

    // Write the combined buffer to m_audioBuffer with wrap-around handling
    size_t firstChunkSize = std::min(totalSize, AUDIO_BUFFER_SIZE - m_writePos);
    memcpy(m_audioBuffer + m_writePos, tempBuffer, firstChunkSize);
    m_writePos = (m_writePos + firstChunkSize) % AUDIO_BUFFER_SIZE;
    m_bufferFilled += firstChunkSize;

    if (firstChunkSize < totalSize)
    {
        size_t secondChunkSize = totalSize - firstChunkSize;
        memcpy(m_audioBuffer, tempBuffer + firstChunkSize, secondChunkSize);
        m_writePos = secondChunkSize;
        m_bufferFilled += secondChunkSize;
    }
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
        m_currentFilePath = "";
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
    m_currentFileIndex = getFileIndex(m_currentFilePath);
    m_isAudioPlaying = true;

    // Reset playback time since we're starting a new file
    m_currentPlaybackTime = 0;
    m_lastFrameTime = millis();

    // Call the playback start callback if set and we have a valid file path
    if (m_playbackStartCallback && !m_currentFilePath.isEmpty())
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
    return addFileToList(filePath); // This now both adds (if new) and returns the index
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

uint16_t AudioPlayer::addFileToList(const String &filePath)
{
    auto it = std::find(m_fileList.begin(), m_fileList.end(), filePath);
    if (it == m_fileList.end())
    {
        m_fileList.push_back(filePath);
        return m_fileList.size() - 1;
    }

    // Return the index of the file
    return std::distance(m_fileList.begin(), it);
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

bool AudioPlayer::hasRemainingAudioData()
{
    return audioFile && audioFile.available();
}