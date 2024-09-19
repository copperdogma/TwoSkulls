/*
    This is the audio player for the skull.

    Note: Frame is defined in SoundData.h in https://github.com/pschatzmann/ESP32-A2DP like so:

    struct __attribute__((packed)) Frame {
        int16_t channel1;
        int16_t channel2;

        Frame(int v=0){
            channel1 = channel2 = v;
        }

        Frame(int ch1, int ch2){
            channel1 = ch1;
            channel2 = ch2;
        }
    };
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
      m_currentBufferFileIndex(0),
      m_currentPlaybackFileIndex(0)
{
}

void AudioPlayer::begin()
{
}

void AudioPlayer::playNext(const char *filePath)
{
    if (filePath)
    {
        std::lock_guard<std::mutex> lock(m_mutex); // Acquire mutex lock
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

    // Handle FileEntry tracking
    if (!m_fileList.empty())
    {
        FileEntry &currentEntry = m_fileList[m_currentPlaybackFileIndex];

        // Check if bufferEndPos is defined and readPos has reached it
        if (currentEntry.bufferEndPos != BUFFER_END_POS_UNDEFINED && m_readPos >= currentEntry.bufferEndPos)
        {
            Serial.printf("AudioPlayer::provideAudioFrames() found END OF FILE at m_readPos (%zu) >= currentEntry.bufferEndPos (%zu) for file: %s\n",
                          m_readPos, currentEntry.bufferEndPos, currentEntry.filePath.c_str());

            // Trigger playbackEndCallback for the current file
            if (m_playbackEndCallback)
            {
                m_playbackEndCallback(currentEntry.filePath);
            }

            // Move to the next file in the list
            m_currentPlaybackFileIndex++;
            if (m_currentPlaybackFileIndex < m_fileList.size())
            {
                // Trigger playbackStartCallback for the next file
                if (m_playbackStartCallback)
                {
                    m_playbackStartCallback(m_fileList[m_currentPlaybackFileIndex].filePath);
                }
            }
            else
            {
                // No more files to play
                m_isAudioPlaying = false;
                m_currentFilePath = "";
                Serial.println("AudioPlayer::provideAudioFrames() No more files to play");
            }
        }
    }

    m_totalBytesRead += bytesRead;

    fillBuffer();

    // Update playback status and time
    m_isAudioPlaying = (bytesRead > 0 || m_bufferFilled > 0);

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
        if (!audioFile || !audioFile.available())
        {
            // We're on the last data from the current file
            if (audioFile)
            {
                writeToBuffer(nullptr, 0);
                String currentFilePath = getFilePath(m_currentBufferFileIndex);
                Serial.printf("AudioPlayer::fillBuffer() found END OF FILE (1) for fileIndex: %d, writePos: %zu, filePath: %s\n",
                              m_currentBufferFileIndex, m_writePos, currentFilePath.c_str());

                m_fileList[m_currentBufferFileIndex].bufferEndPos = m_writePos;

                audioFile.close();
            }

            // Start the next file
            if (!startNextFile())
            {
                break;
            }
            writeToBuffer(nullptr, 0);
            String currentFilePath = getFilePath(m_currentBufferFileIndex);
            Serial.printf("AudioPlayer::fillBuffer() starting from NEW FILE: fileIndex: %d, filePath: %s\n",
                          m_currentBufferFileIndex, currentFilePath.c_str());
        }
        else
        {
            uint8_t audioData[512];
            size_t bytesRead = audioFile.read(audioData, sizeof(audioData));
            if (bytesRead > 0)
            {
                writeToBuffer(audioData, bytesRead);
            }
            else
            {
                // What is this section? Another end of file? Why?
                writeToBuffer(nullptr, 0);
                String currentFilePath = getFilePath(m_currentBufferFileIndex);
                Serial.printf("AudioPlayer::fillBuffer() found END OF FILE (2) for fileIndex: %d, writePos: %zu, filePath: %s\n",
                              m_currentBufferFileIndex, m_writePos, currentFilePath.c_str());

                m_fileList[m_currentBufferFileIndex].bufferEndPos = m_writePos;

                audioFile.close();
            }
        }
    }
}

void AudioPlayer::writeToBuffer(const uint8_t *audioData, size_t dataSize)
{
    size_t spaceAvailable = AUDIO_BUFFER_SIZE - m_bufferFilled;

    // Write audio data if available
    if (audioData && dataSize > 0 && spaceAvailable > 0)
    {
        size_t bytesToWrite = std::min(dataSize, spaceAvailable);
        size_t firstChunkSize = std::min(bytesToWrite, AUDIO_BUFFER_SIZE - m_writePos);
        memcpy(m_audioBuffer + m_writePos, audioData, firstChunkSize);
        m_writePos = (m_writePos + firstChunkSize) % AUDIO_BUFFER_SIZE;
        m_bufferFilled += firstChunkSize;

        if (firstChunkSize < bytesToWrite)
        {
            size_t secondChunkSize = bytesToWrite - firstChunkSize;
            memcpy(m_audioBuffer, audioData + firstChunkSize, secondChunkSize);
            m_writePos = secondChunkSize;
            m_bufferFilled += secondChunkSize;
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

    // TODO: try putting this to 128 to skip the full header.
    // Skip WAV header (44 bytes)
    audioFile.seek(44);

    m_currentFilePath = String(nextFile.c_str());
    m_currentBufferFileIndex = getFileIndex(m_currentFilePath);

    Serial.printf("AudioPlayer: Started buffering new file: %s (index: %d)\n", m_currentFilePath.c_str(), m_currentBufferFileIndex);
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
        return m_fileList[fileIndex].filePath;
    }
    else
    {
        return "";
    }
}

uint16_t AudioPlayer::addFileToList(const String &filePath)
{
    auto it = std::find_if(m_fileList.begin(), m_fileList.end(),
                           [&filePath](const FileEntry &entry)
                           { return entry.filePath == filePath; });
    if (it == m_fileList.end())
    {
        m_fileList.emplace_back(filePath, BUFFER_END_POS_UNDEFINED); // Initialize bufferEndPos to undefined
        return m_fileList.size() - 1;
    }

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