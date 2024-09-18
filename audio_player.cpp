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
        // Read the file index with buffer wrap-around handling
        uint16_t fileIndex;
        if (m_readPos + sizeof(uint16_t) <= AUDIO_BUFFER_SIZE)
        {
            memcpy(&fileIndex, m_audioBuffer + m_readPos, sizeof(uint16_t));
        }
        else
        {
            size_t firstPartSize = AUDIO_BUFFER_SIZE - m_readPos;
            memcpy(&fileIndex, m_audioBuffer + m_readPos, firstPartSize);
            memcpy(((uint8_t *)&fileIndex) + firstPartSize, m_audioBuffer, sizeof(uint16_t) - firstPartSize);
        }
        m_readPos = (m_readPos + sizeof(uint16_t)) % AUDIO_BUFFER_SIZE;
        m_bufferFilled -= sizeof(uint16_t);

        if (fileIndex != SAME_FILE)
        {
            if (fileIndex == END_OF_FILE)
            {
                // Call playback end callback
                if (m_playbackEndCallback)
                {
                    m_playbackEndCallback(m_currentFilePath);
                }
                Serial.printf("AudioPlayer: Playback ended for file: %s\n", m_currentFilePath);
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
                // Serial.printf("AudioPlayer: Playback started for file: %s\n", m_currentFilePath);
            }
        }

        // Read audio data with buffer wrap-around handling
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
        uint16_t fileIndex = SAME_FILE;

        if (!audioFile || !audioFile.available())
        {
            if (m_bufferFilled == 0)
            {
                if (!startNextFile())
                {
                    fileIndex = END_OF_FILE;
                    writeToBuffer(fileIndex, nullptr, 0);
                    break; // No more files to play
                }
                fileIndex = m_currentFileIndex; // Use the current file index directly
            }
            else
            {
                fileIndex = END_OF_FILE;
            }
        }

    // Debugging:
        // switch (fileIndex) {
        //     case SAME_FILE: Serial.println("AudioPlayer:fillBuffer(): writing fileIndex SAME_FILE"); break;
        //     case END_OF_FILE: Serial.println("AudioPlayer:fillBuffer(): writing fileIndex END_OF_FILE"); break;
        //     default: Serial.printf("AudioPlayer:fillBuffer(): writing actively playing file index %d\n", fileIndex); break;
        // }

        if (audioFile && audioFile.available())
        {
            size_t spaceAvailable = AUDIO_BUFFER_SIZE - m_bufferFilled;
            const size_t MAX_READ_SIZE = 512; // Define a reasonable maximum read size
            size_t bytesToRead = std::min({spaceAvailable, (size_t)audioFile.available(), MAX_READ_SIZE});

            uint8_t audioData[MAX_READ_SIZE]; // Fixed-size buffer
            size_t bytesRead = audioFile.read(audioData, bytesToRead);

            if (bytesRead > 0)
            {
                writeToBuffer(fileIndex, audioData, bytesRead);
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

void AudioPlayer::writeToBuffer(uint16_t fileIndex, const uint8_t *audioData, size_t dataSize)
{
    // Write the file index with buffer wrap-around handling
    if (m_writePos + sizeof(uint16_t) <= AUDIO_BUFFER_SIZE)
    {
        memcpy(m_audioBuffer + m_writePos, &fileIndex, sizeof(uint16_t));
    }
    else
    {
        // FileIndex will wrap around the buffer
        size_t firstPartSize = AUDIO_BUFFER_SIZE - m_writePos;
        memcpy(m_audioBuffer + m_writePos, &fileIndex, firstPartSize);
        memcpy(m_audioBuffer, ((uint8_t *)&fileIndex) + firstPartSize, sizeof(uint16_t) - firstPartSize);
    }
    m_writePos = (m_writePos + sizeof(uint16_t)) % AUDIO_BUFFER_SIZE;
    m_bufferFilled += sizeof(uint16_t);

    if (audioData && dataSize > 0)
    {
        // Handle buffer wrap-around for audio data
        size_t firstChunkSize = std::min(dataSize, AUDIO_BUFFER_SIZE - m_writePos);
        memcpy(m_audioBuffer + m_writePos, audioData, firstChunkSize);
        m_writePos = (m_writePos + firstChunkSize) % AUDIO_BUFFER_SIZE;
        m_bufferFilled += firstChunkSize;

        if (firstChunkSize < dataSize)
        {
            size_t secondChunkSize = dataSize - firstChunkSize;
            memcpy(m_audioBuffer, audioData + firstChunkSize, secondChunkSize);
            m_writePos = secondChunkSize;
            m_bufferFilled += secondChunkSize;
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