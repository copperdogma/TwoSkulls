#include "audio_player.h"
#include "file_manager.h"
#include <cmath>
#include <algorithm>
#include <Arduino.h>
#include <esp_task_wdt.h>
#include <mutex>
#include <thread>

AudioPlayer::AudioPlayer() 
    : m_totalBytesRead(0), m_isBluetoothConnected(false), m_isAudioReadyToPlay(false),
      m_writePos(0), m_readPos(0), m_bufferFilled(0) {
}

void AudioPlayer::begin() {
    // Initialize audio player
    xTaskCreatePinnedToCore(
        [](void* parameter) {
            static_cast<AudioPlayer*>(parameter)->audioPlayerTask();
        },
        "AudioPlayerTask",
        8192,  // Increase stack size to 8KB
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
        writeAudio();  // Start filling the buffer immediately
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
        playFile(nextFile.c_str());
    }
    queueCV.notify_one();
}

bool AudioPlayer::isCurrentlyPlaying() {
    return audioFile && audioFile.available();
}

bool AudioPlayer::hasFinishedPlaying() {
    return !audioFile || !audioFile.available();
}

void AudioPlayer::setBluetoothConnected(bool connected) {
    Serial.printf("Bluetooth connection status changed to: %s\n", connected ? "connected" : "disconnected");
    m_isBluetoothConnected = connected;
    if (connected) {
        m_bufferPosition = 0;  // Reset buffer position to start from the beginning
        if (!isCurrentlyPlaying() && !audioQueue.empty()) {
            // Start playing the first file in the queue if not already playing
            std::string filePath = audioQueue.front();
            audioQueue.pop();
            playFile(filePath.c_str());
        }
    }
}

void AudioPlayer::setAudioReadyToPlay(bool ready) {
    m_isAudioReadyToPlay = ready;
}

void AudioPlayer::logState() {
    Serial.println("AudioPlayer State:");
    Serial.printf("  Is playing: %d\n", isCurrentlyPlaying());
    Serial.printf("  Total bytes read: %d\n", m_totalBytesRead);
    Serial.printf("  Bluetooth connected: %d\n", m_isBluetoothConnected);
    Serial.printf("  Audio ready to play: %d\n", m_isAudioReadyToPlay);
}

bool AudioPlayer::fileExists(fs::FS& fs, const char* path) {
    return fs.exists(path);
}

size_t AudioPlayer::getTotalBytesRead() const {
    return m_totalBytesRead;
}

void AudioPlayer::setBluetoothCallback(std::function<int32_t(Frame*, int32_t)> callback) {
    m_bluetoothCallback = callback;
}

int32_t AudioPlayer::provideAudioFrames(Frame* frame, int32_t frame_count) {
    if (!m_isBluetoothConnected) {
        memset(frame, 0, frame_count * sizeof(Frame));
        return frame_count;
    }

    size_t bytesToProvide = frame_count * sizeof(Frame);
    size_t bytesProvided = 0;

    while (bytesProvided < bytesToProvide) {
        if (m_bufferFilled == 0) {
            writeAudio();
            if (m_bufferFilled == 0) {
                // End of file reached
                memset((uint8_t*)frame + bytesProvided, 0, bytesToProvide - bytesProvided);
                return bytesProvided / sizeof(Frame);
            }
        }

        size_t bytesToCopy = min(bytesToProvide - bytesProvided, m_bufferFilled);
        if (m_readPos + bytesToCopy > AUDIO_BUFFER_SIZE) {
            size_t firstPart = AUDIO_BUFFER_SIZE - m_readPos;
            size_t secondPart = bytesToCopy - firstPart;
            memcpy((uint8_t*)frame + bytesProvided, m_audioBuffer + m_readPos, firstPart);
            memcpy((uint8_t*)frame + bytesProvided + firstPart, m_audioBuffer, secondPart);
            m_readPos = secondPart;
        } else {
            memcpy((uint8_t*)frame + bytesProvided, m_audioBuffer + m_readPos, bytesToCopy);
            m_readPos = (m_readPos + bytesToCopy) % AUDIO_BUFFER_SIZE;
        }

        bytesProvided += bytesToCopy;
        m_bufferFilled -= bytesToCopy;
        m_totalBytesRead += bytesToCopy;
    }

    return frame_count;
}

// Remove getCurrentAudioBuffer and getCurrentAudioBufferSize methods

size_t AudioPlayer::readAudioData(uint8_t* buffer, size_t bytesToRead) {
    if (!audioFile || !audioFile.available()) {
        // Try to reopen the file
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

void AudioPlayer::incrementTotalBytesRead(size_t bytesRead) {
    m_totalBytesRead += bytesRead;
}

void AudioPlayer::writeAudio() {
    if (!audioFile) {
        Serial.println("AudioPlayer: writeAudio called with null audioFile");
        return;
    }

    size_t spaceAvailable = AUDIO_BUFFER_SIZE - m_bufferFilled;
    size_t writeAmount = std::min(spaceAvailable, static_cast<size_t>(audioFile.available()));
    
    Serial.printf("AudioPlayer: Writing audio. Space available: %d, Write amount: %d\n", spaceAvailable, writeAmount);
    
    if (writeAmount == 0) {
        Serial.println("AudioPlayer: No data to write");
        return;
    }

    if (m_writePos + writeAmount > AUDIO_BUFFER_SIZE) {
        size_t firstPart = AUDIO_BUFFER_SIZE - m_writePos;
        size_t secondPart = writeAmount - firstPart;
        size_t bytesRead1 = audioFile.read(m_audioBuffer + m_writePos, firstPart);
        size_t bytesRead2 = audioFile.read(m_audioBuffer, secondPart);
        m_writePos = secondPart;
        Serial.printf("AudioPlayer: Split write. First part: %d, Second part: %d\n", bytesRead1, bytesRead2);
        if (bytesRead1 != firstPart || bytesRead2 != secondPart) {
            Serial.println("AudioPlayer: Error reading file in split write");
        }
    } else {
        size_t bytesRead = audioFile.read(m_audioBuffer + m_writePos, writeAmount);
        m_writePos = (m_writePos + writeAmount) % AUDIO_BUFFER_SIZE;
        Serial.printf("AudioPlayer: Single write. Bytes read: %d\n", bytesRead);
        if (bytesRead != writeAmount) {
            Serial.println("AudioPlayer: Error reading file in single write");
        }
    }
    
    m_bufferFilled += writeAmount;
    Serial.printf("AudioPlayer: Audio written. New buffer filled: %d bytes\n", m_bufferFilled);
}

void AudioPlayer::audioPlayerTask() {
    while (!shouldStop) {
        try {
            if (isPlaying) {
                if (!isCurrentlyPlaying()) {
                    isPlaying = false;
                    Serial.printf("AudioPlayer: Finished playback of file\n");
                    
                    std::unique_lock<std::mutex> lock(queueMutex);
                    if (!audioQueue.empty()) {
                        std::string nextFile = audioQueue.front();
                        audioQueue.pop();
                        Serial.printf("AudioPlayer: Starting next file in queue: %s\n", nextFile.c_str());
                        lock.unlock();
                        playFile(nextFile.c_str());
                    } else {
                        Serial.println("AudioPlayer: No more files in queue");
                    }
                }
            } else {
                std::unique_lock<std::mutex> lock(queueMutex);
                if (!audioQueue.empty()) {
                    std::string nextFile = audioQueue.front();
                    audioQueue.pop();
                    Serial.printf("AudioPlayer: Starting file from idle state: %s\n", nextFile.c_str());
                    lock.unlock();
                    playFile(nextFile.c_str());
                } else {
                    queueCV.wait(lock, [this] { return !audioQueue.empty() || shouldStop; });
                }
            }
        } catch (const std::exception& e) {
            Serial.printf("AudioPlayer: Exception caught in audioPlayerTask: %s\n", e.what());
            isPlaying = false;
        } catch (...) {
            Serial.println("AudioPlayer: Unknown exception caught in audioPlayerTask");
            isPlaying = false;
        }
        delay(10);  // Small delay to prevent tight looping
    }
    Serial.println("AudioPlayer: audioPlayerTask stopping");
}

void AudioPlayer::playFile(const char* filePath) {
    Serial.printf("AudioPlayer: Starting playback of file: %s\n", filePath);
    
    // Ensure the previous file is closed
    if (audioFile) {
        audioFile.close();
        Serial.println("AudioPlayer: Previous file closed");
    }
    
    // Reset buffer and state
    memset(m_audioBuffer, 0, AUDIO_BUFFER_SIZE);
    m_writePos = 0;
    m_readPos = 0;
    m_bufferFilled = 0;
    m_totalBytesRead = 0;
    
    Serial.printf("AudioPlayer: Buffer reset. Free heap: %d\n", ESP.getFreeHeap());
    
    // Check if file exists before opening
    if (!SD.exists(filePath)) {
        Serial.printf("AudioPlayer: File does not exist: %s\n", filePath);
        isPlaying = false;
        return;
    }
    
    // Open the new file
    audioFile = SD.open(filePath, FILE_READ);
    if (!audioFile) {
        Serial.printf("AudioPlayer: Failed to open audio file: %s\n", filePath);
        isPlaying = false;
        return;
    }
    
    Serial.printf("AudioPlayer: File opened successfully. File size: %d bytes\n", audioFile.size());
    
    isPlaying = true;
    
    // Start filling the buffer
    try {
        writeAudio();
        Serial.printf("AudioPlayer: Initial buffer filled: %d bytes\n", m_bufferFilled);
    } catch (const std::exception& e) {
        Serial.printf("AudioPlayer: Exception in writeAudio: %s\n", e.what());
        isPlaying = false;
    } catch (...) {
        Serial.println("AudioPlayer: Unknown exception in writeAudio");
        isPlaying = false;
    }
}