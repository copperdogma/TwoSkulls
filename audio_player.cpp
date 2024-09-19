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

//CAMILL: out for now
    // // Process the frames and handle fileIndex
    // size_t processedBytes = 0;
    // int32_t framesProcessed = 0;
    // while (processedBytes < bytesRead && framesProcessed < frame_count)
    // {
    //     uint16_t fileIndex;
    //     memcpy(&fileIndex, (uint8_t *)frame + processedBytes, sizeof(uint16_t));

    //     switch (fileIndex)
    //     {
    //     case SAME_FILE:
    //         break;
    //     case END_OF_FILE:
    //     {
    //         if (m_playbackEndCallback)
    //         {
    //             m_playbackEndCallback(m_currentFilePath);
    //         }
    //         m_currentPlaybackFileIndex = 0;
    //         m_currentFilePath = "";
    //         m_currentPlaybackTime = 0;
    //         m_lastFrameTime = 0;
    //         break;
    //     }

    //     // New file
    //     default:
    //     {
    //         m_currentPlaybackFileIndex = fileIndex;
    //         m_currentFilePath = getFilePath(m_currentPlaybackFileIndex);
    //         if (m_playbackStartCallback && !m_currentFilePath.isEmpty())
    //         {
    //             m_playbackStartCallback(m_currentFilePath);
    //         }
    //         m_currentPlaybackTime = 0;
    //         m_lastFrameTime = millis();
    //         break;
    //     }
    //     }

    //     processedBytes += sizeof(Frame);
    //     framesProcessed++;
    // }

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
            if (audioFile)
            {
                writeToBuffer(END_OF_FILE, nullptr, 0);
                String currentFilePath = getFilePath(m_currentBufferFileIndex);
                Serial.printf("AudioPlayer::fillBuffer() writing END_OF_FILE for fileIndex: %d, filePath: %s\n", m_currentBufferFileIndex, currentFilePath.c_str());
                audioFile.close();
            }
            if (!startNextFile())
            {
                break;
            }
            writeToBuffer(m_currentBufferFileIndex, nullptr, 0);
            String currentFilePath = getFilePath(m_currentBufferFileIndex);
            Serial.printf("AudioPlayer::fillBuffer() new file: writing m_currentBufferFileIndex: %d, filePath: %s\n", m_currentBufferFileIndex, currentFilePath.c_str());
        }
        else
        {
            uint8_t audioData[512];
            size_t bytesRead = audioFile.read(audioData, sizeof(audioData));
            if (bytesRead > 0)
            {
                writeToBuffer(SAME_FILE, audioData, bytesRead);
            }
            else
            {
                writeToBuffer(END_OF_FILE, nullptr, 0);
                String currentFilePath = getFilePath(m_currentBufferFileIndex);
                Serial.printf("AudioPlayer::fillBuffer() writing END_OF_FILE for m_currentBufferFileIndex: %d, filePath: %s\n", m_currentBufferFileIndex, currentFilePath.c_str());
                audioFile.close();
            }
        }
    }
}

//CAMKILL: fileIndex is still in method signature but is no longer used
void AudioPlayer::writeToBuffer(uint16_t fileIndex, const uint8_t *audioData, size_t dataSize)
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