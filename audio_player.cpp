#include "audio_player.h"
#include "file_manager.h"
#include <cmath>
#include <algorithm>
#include <Arduino.h>
#include <esp_task_wdt.h>
#include <mutex>
#include <thread>

AudioPlayer::AudioPlayer() 
    : m_totalBytesRead(0), m_isBluetoothConnected(false),
      m_writePos(0), m_readPos(0), m_bufferFilled(0), m_currentFilePath(""),
      m_isAudioPlaying(false) {
}

void AudioPlayer::begin() {
    xTaskCreatePinnedToCore(
        [](void* parameter) {
            static_cast<AudioPlayer*>(parameter)->audioPlayerTask();
        },
        "AudioPlayerTask",
        8192,
        this,
        1,
        NULL,
        1
    );
}

void AudioPlayer::update() {
    if (audioFile && m_bufferFilled < AUDIO_BUFFER_SIZE / 2) {
        writeAudio();
    }
}

void AudioPlayer::playNow(const char* filePath) {
    if (audioFile) {
        audioFile.close();
    }
    
    audioFile = SD.open(filePath);
    if (!audioFile) {
        Serial.printf("Failed to open audio file: %s\n", filePath);
    } else {
        m_totalBytesRead = 0;
        m_writePos = 0;
        m_readPos = 0;
        m_bufferFilled = 0;
        m_playbackStartTime = millis();
        writeAudio();
    }
}

void AudioPlayer::playNext(const char* filePath) {
    std::lock_guard<std::mutex> lock(queueMutex);
    if (filePath) {
        audioQueue.push(std::string(filePath));
        Serial.printf("AudioPlayer: Added file to queue: %s (New queue size: %d)\n", filePath, audioQueue.size());
    }
    if (!isPlaying && !audioQueue.empty()) {
        std::string nextFile = audioQueue.front();
        audioQueue.pop();
        Serial.printf("AudioPlayer: Starting next file in queue: %s (New queue size: %d)\n", nextFile.c_str(), audioQueue.size());
        playFile(nextFile.c_str());
    }
    queueCV.notify_one();
}

size_t AudioPlayer::getTotalBytesRead() const {
    return m_totalBytesRead;
}

void AudioPlayer::incrementTotalBytesRead(size_t bytesRead) {
    m_totalBytesRead += bytesRead;
}

size_t AudioPlayer::readAudioData(uint8_t* buffer, size_t bytesToRead) {
    if (!audioFile || !audioFile.available()) {
        String currentFilePath = audioFile.name();
        audioFile = SD.open(currentFilePath.c_str());
        if (!audioFile) {
            return 0;
        }
    }

    if (audioFile && audioFile.available()) {
        size_t bytesRead = audioFile.read(buffer, bytesToRead);
        return bytesRead;
    }
    
    return 0;
}

void AudioPlayer::writeAudio() {
    if (!audioFile) {
        Serial.println("AudioPlayer: writeAudio called with null audioFile");
        return;
    }

    size_t spaceAvailable = AUDIO_BUFFER_SIZE - m_bufferFilled;
    size_t writeAmount = std::min(spaceAvailable, static_cast<size_t>(audioFile.available()));
    
    if (writeAmount == 0) {
        return;
    }

    if (m_writePos + writeAmount > AUDIO_BUFFER_SIZE) {
        size_t firstPart = AUDIO_BUFFER_SIZE - m_writePos;
        size_t secondPart = writeAmount - firstPart;
        size_t bytesRead1 = audioFile.read(m_audioBuffer + m_writePos, firstPart);
        size_t bytesRead2 = audioFile.read(m_audioBuffer, secondPart);
        m_writePos = secondPart;
        if (bytesRead1 != firstPart || bytesRead2 != secondPart) {
            Serial.println("AudioPlayer: Error reading file in split write");
        }
    } else {
        size_t bytesRead = audioFile.read(m_audioBuffer + m_writePos, writeAmount);
        m_writePos = (m_writePos + writeAmount) % AUDIO_BUFFER_SIZE;
        if (bytesRead != writeAmount) {
            Serial.println("AudioPlayer: Error reading file in single write");
        }
    }
    
    m_bufferFilled += writeAmount;
}

void AudioPlayer::audioPlayerTask() {
    while (!shouldStop) {
        try {
            if (isPlaying) {
                if (!hasRemainingAudioData()) {
                    isPlaying = false;
                    m_isAudioPlaying = false;
                    Serial.printf("AudioPlayer: Finished playback of file\n");

                    m_currentFilePath = "";
                    
                    std::unique_lock<std::mutex> lock(queueMutex);
                    if (!audioQueue.empty()) {
                        std::string nextFile = audioQueue.front();
                        audioQueue.pop();
                        Serial.printf("AudioPlayer: Starting next file in queue: %s\n", nextFile.c_str());
                        lock.unlock();
                        playFile(nextFile.c_str());
                    }
                }
                else {
                    m_isAudioPlaying = true;
                }
            } else {
                m_isAudioPlaying = false;
            }
            // ... rest of the method ...
        } catch (const std::exception& e) {
            Serial.printf("AudioPlayer: Exception caught in audioPlayerTask: %s\n", e.what());
            isPlaying = false;
        } catch (...) {
            Serial.println("AudioPlayer: Unknown exception caught in audioPlayerTask");
            isPlaying = false;
        }
        delay(10);
    }
}

// ** ISSUES:
// - it decides it's finished playing too early on the init.wav so it never turns on the lights
// - can we kill isPlaying? Just replace it with m_isAudioPlaying?

void AudioPlayer::playFile(const char* filePath) {
    Serial.printf("AudioPlayer: Starting playback of file: %s\n", filePath);
    
    if (audioFile) {
        audioFile.close();
    }
    
    memset(m_audioBuffer, 0, AUDIO_BUFFER_SIZE);
    m_writePos = 0;
    m_readPos = 0;
    m_bufferFilled = 0;
    m_totalBytesRead = 0;
    
    if (!SD.exists(filePath)) {
        Serial.printf("AudioPlayer: File does not exist: %s\n", filePath);
        isPlaying = false;
        return;
    }
    
    audioFile = SD.open(filePath, FILE_READ);
    if (!audioFile) {
        Serial.printf("AudioPlayer: Failed to open audio file: %s\n", filePath);
        isPlaying = false;
        return;
    }

    // CAMKILL: If you remove this it instantly thinks it's finished playing.
    // I'm not sure why. I'm not sure I like the flow of this code.
    // It's not that easy to follow and some of the "playing" stuff is a bit counterintuitive.
    // We can't actually say we're PLAYING right now because we can't play until we're connected to bluetooth.
    // So this feels more like we should have multiple states: EMPTY, DATA_READY, PLAYING, PAUSED, etc.
    isPlaying = true;

    m_currentFilePath = filePath;  // Update the current file path
    m_currentFileStartTime = millis();  // Reset the start time for the new file

    try {
        writeAudio();
    } catch (const std::exception& e) {
        Serial.printf("AudioPlayer: Exception in writeAudio: %s\n", e.what());
        isPlaying = false;
    } catch (...) {
        Serial.println("AudioPlayer: Unknown exception in writeAudio");
        isPlaying = false;
    }
}

bool AudioPlayer::hasRemainingAudioData() {
    return audioFile && audioFile.available();
}

bool AudioPlayer::isAudioPlaying() const {
    return m_isAudioPlaying;
}

unsigned long AudioPlayer::getPlaybackTime() const {
    if (!isPlaying) {
        return 0;
    }
    return millis() - m_currentFileStartTime;
}

String AudioPlayer::getCurrentlyPlayingFilePath() const {
    return m_currentFilePath;
}

int32_t AudioPlayer::provideAudioFrames(Frame *frame, int32_t frame_count)
{
    if (!hasRemainingAudioData())
    {
        m_isAudioPlaying = false;
        memset(frame, 0, frame_count * sizeof(Frame));
        return frame_count;
    }

    m_isAudioPlaying = true;

    size_t bytesToRead = frame_count * sizeof(Frame);
    size_t bytesRead = readAudioData((uint8_t *)frame, bytesToRead);

    if (bytesRead < bytesToRead)
    {
        memset((uint8_t *)frame + bytesRead, 0, bytesToRead - bytesRead);

        if (bytesRead == 0)
        {
            Serial.println("AudioPlayer: End of file reached, attempting to play next file");
            playNext(nullptr);
        }
    }

    incrementTotalBytesRead(bytesRead);

    return frame_count;
}