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
#include "radio_manager.h"

constexpr size_t AudioPlayer::BUFFER_END_POS_UNDEFINED;
static unsigned long lastPrintedSecond = 0;

AudioPlayer::AudioPlayer(SDCardManager &sdCardManager, RadioManager &radioManager)
    : m_writePos(0), m_readPos(0), m_bufferFilled(0),
      m_currentFilePath(""), m_isAudioPlaying(false), m_muted(false),
      m_currentPlaybackTime(0), m_lastFrameTime(0),
      m_currentBufferFileIndex(0), m_currentPlaybackFileIndex(0),
      m_sdCardManager(sdCardManager), m_radioManager(radioManager)
{
}

void AudioPlayer::playNext(String filePath)
{
    if (filePath.length() > 0)
    {
        std::lock_guard<std::mutex> lock(m_mutex); // Acquire mutex lock
        uint16_t newFileIndex = addFileToList(filePath); // Add the file to the list and get the index
        audioQueue.push(filePath.c_str());
        Serial.printf("AudioPlayer::playNext() Added file (index: %d) to queue: %s ...\n", newFileIndex, filePath.c_str());
    }
}

int32_t AudioPlayer::provideAudioFrames(Frame *frame, int32_t frame_count)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Exit if there's no data available to read.
    if (m_bufferFilled == 0)
    {
        m_isAudioPlaying = false;
        fillBuffer();
        return 0;
    }

    // Request radio access
    if (!m_radioManager.requestAccess(IDENTIFIER, RADIO_ACCESS_TIMEOUT_MS))
    {
        Serial.println("AudioPlayer::provideAudioFrames() Couldn't get radio access");
        return 0;
    }

    size_t bytesToRead = frame_count * sizeof(Frame);
    size_t bytesRead = 0;

    // Read data from buffer
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
    if (!m_fileList.empty() && m_currentPlaybackFileIndex < m_fileList.size())
    {
        FileEntry &currentEntry = m_fileList[m_currentPlaybackFileIndex];

        // Check if we've reached the end of the current file
        if (currentEntry.bufferEndPos != BUFFER_END_POS_UNDEFINED && m_readPos >= currentEntry.bufferEndPos)
        {
            handleEndOfFile();
        }
    }

    fillBuffer();

    // Update playback status and time
    m_isAudioPlaying = (bytesRead > 0 || m_bufferFilled > 0);

    if (m_muted)
    {
        memset(frame, 0, frame_count * sizeof(Frame));
    }

    updatePlaybackTime();

    // Call the frames provided callback if set
    if (m_audioFramesProvidedCallback)
    {
        m_audioFramesProvidedCallback(m_currentFilePath, frame, frame_count);
    }

    // No need to explicitly release radio access
    return frame_count;
}

void AudioPlayer::handleEndOfFile()
{
    // Check if we're within bounds of the file list
    if (m_currentPlaybackFileIndex >= m_fileList.size()) {
        Serial.println("AudioPlayer::handleEndOfFile() No more files to play");
        m_isAudioPlaying = false;
        m_currentFilePath = "";
        return;
    }

    FileEntry &currentEntry = m_fileList[m_currentPlaybackFileIndex];
    Serial.printf("AudioPlayer::handleEndOfFile() found END OF FILE at m_readPos (%zu) >= currentEntry.bufferEndPos (%zu) for file: %s\n",
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

        // Start playback of the next file
        m_isAudioPlaying = true;
        m_currentFilePath = m_fileList[m_currentPlaybackFileIndex].filePath;
        Serial.printf("AudioPlayer::handleEndOfFile() Starting playback of next file: %s\n", m_currentFilePath.c_str());
    }
    else
    {
        // No more files to play
        m_isAudioPlaying = false;
        m_currentFilePath = "";
        Serial.println("AudioPlayer::handleEndOfFile() No more files to play");
    }
}

// New helper method to update playback time
void AudioPlayer::updatePlaybackTime()
{
    unsigned long now = millis();
    if (m_lastFrameTime != 0)
    {
        unsigned long elapsedTime = now - m_lastFrameTime;
        m_currentPlaybackTime += elapsedTime;
    }
    m_lastFrameTime = now;
    
    // Only print every second
    if (m_currentPlaybackTime / 1000 != lastPrintedSecond)
    {
        lastPrintedSecond = m_currentPlaybackTime / 1000;
        Serial.printf("AudioPlayer::updatePlaybackTime() m_currentPlaybackTime: %lu\n", m_currentPlaybackTime);
    }
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

    audioFile = m_sdCardManager.openFile(nextFile.c_str());
    if (!audioFile)
    {
        Serial.printf("AudioPlayer::startNextFile() Failed to open audio file: %s\n", nextFile.c_str());
        return startNextFile(); // Try the next file in the queue
    }

    // Skip WAV header.
    // 44 bytes is minimum, the skull files have closer to 128 bytes, and skipping a bit more just skips some blank audio at the start.
    // Not skipping enough header will cause the header to be played, often rewsulting in a click at the start of the audio.
    // There's a proper way to parse out the header, but it's involved and this is good enough.
    audioFile.seek(128);

    m_currentFilePath = String(nextFile.c_str());
    m_currentBufferFileIndex = getFileIndex(m_currentFilePath);

    Serial.printf("AudioPlayer::startNextFile() Started buffering new file: %s (index: %d)\n", m_currentFilePath.c_str(), m_currentBufferFileIndex);
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