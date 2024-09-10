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
    std::thread(&AudioPlayer::audioPlayerTask, this).detach();
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
    size_t spaceAvailable = AUDIO_BUFFER_SIZE - m_bufferFilled;
    size_t writeAmount = std::min(spaceAvailable, static_cast<size_t>(audioFile.available()));
    
    if (m_writePos + writeAmount > AUDIO_BUFFER_SIZE) {
        size_t firstPart = AUDIO_BUFFER_SIZE - m_writePos;
        size_t secondPart = writeAmount - firstPart;
        audioFile.read(m_audioBuffer + m_writePos, firstPart);
        audioFile.read(m_audioBuffer, secondPart);
        m_writePos = secondPart;
    } else {
        audioFile.read(m_audioBuffer + m_writePos, writeAmount);
        m_writePos = (m_writePos + writeAmount) % AUDIO_BUFFER_SIZE;
    }
    
    m_bufferFilled += writeAmount;
}

void AudioPlayer::audioPlayerTask() {
    while (true) {
        std::string filePath;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCV.wait(lock, [this] { return !audioQueue.empty() || shouldStop; });
            if (shouldStop) {
                Serial.println("AudioPlayer: audioPlayerTask stopping");
                break;
            }
            filePath = audioQueue.front();
            audioQueue.pop();
        }
        
        Serial.printf("AudioPlayer: Starting playback of file: %s (Remaining queue size: %d)\n", filePath.c_str(), audioQueue.size());
        playFile(filePath.c_str());
    }
}

void AudioPlayer::playFile(const char* filePath) {
    Serial.printf("AudioPlayer: Starting playback of file: %s\n", filePath);
    playNow(filePath);  // Use the existing playNow method to start playback
    isPlaying = true;
    while (isPlaying && !shouldStop && isCurrentlyPlaying()) {
        update();  // Call update to keep filling the buffer
        delay(10);  // Small delay to prevent tight looping
    }
    isPlaying = false;
    Serial.printf("AudioPlayer: Finished playback of file: %s\n", filePath);
}