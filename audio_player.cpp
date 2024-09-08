#include "audio_player.h"
#include "file_manager.h"
#include <cmath>
#include <algorithm>
#include <Arduino.h>
#include <esp_task_wdt.h>  // Include ESP32 watchdog timer library

// Define __guard type
typedef int __guard __attribute__((mode(__DI__)));

// Function to capture stack trace (placeholder)
void captureStackTrace() {
  Serial.println("Capturing stack trace...");
  // Implement platform-specific stack trace capture here
  // For example, you can print the values of important registers
}

// Custom error handler
void customErrorHandler(const char* errorMessage) {
  Serial.println(errorMessage);
  captureStackTrace();
  while (1);  // Halt the system
}

// Override the default abort function to capture the stack trace
extern "C" void __cxa_pure_virtual() {
  customErrorHandler("Pure virtual function called!");
}

AudioPlayer::AudioPlayer(ServoController& servoController) 
    : FFT(vReal, vImag, SAMPLES, SAMPLE_RATE),
      buffer(nullptr), currentBufferSize(0), shouldPlayNow(false), isPlaying(false),
      m_isPlayingSkit(false), m_currentSkitLine(0), m_skitStartTime(0),
      m_hasStartedPlaying(false), m_isBluetoothConnected(false), m_isAudioReadyToPlay(false),
      m_reachedEndOfFile(false),
      m_servoController(servoController),
      m_totalFileSize(0), m_totalBytesRead(0), m_isEndOfFile(false) {}  // Initialize new members

void AudioPlayer::begin() {
    // Initialize audio player
}

void AudioPlayer::update() {
  if (shouldPlayNow) {
    Serial.printf("AudioPlayer: Now playing file: %s\n", currentAudioFile.name());
    startPlaying(currentAudioFile.name());
    shouldPlayNow = false;
  }

  // Remove the queue checking here, as it's now handled in _provideAudioFrames

  updateSkit();

  static unsigned long lastPlaybackProgress = 0;
  static unsigned long lastPlaybackCheck = 0;
  unsigned long currentTime = millis();

  if (isPlaying && currentTime - lastPlaybackCheck > 30000) {  // Check every 30 seconds instead of 5
    if (m_totalBytesRead == lastPlaybackProgress) {
      Serial.println("WARNING: Audio playback seems to be stalled!");
      logState();  // Only log state when stalled
    }
    lastPlaybackProgress = m_totalBytesRead;
    lastPlaybackCheck = currentTime;
  }
}

void AudioPlayer::updateSkit() {
  if (m_isPlayingSkit && m_hasStartedPlaying) {
    if (m_skitStartTime == 0) {
      m_skitStartTime = millis();
    }

    unsigned long currentTime = millis() - m_skitStartTime;

    while (m_currentSkitLine < m_currentSkit.lines.size() && 
           currentTime >= m_currentSkit.lines[m_currentSkitLine].timestamp) {
      processSkitLine();
      m_currentSkitLine++;
    }

    if (m_currentSkitLine >= m_currentSkit.lines.size()) {
      m_isPlayingSkit = false;
    }
  }
}

uint8_t* AudioPlayer::getCurrentAudioBuffer() {
  return buffer;
}

size_t AudioPlayer::getCurrentAudioBufferSize() {
  return currentBufferSize;
}

void AudioPlayer::playNow(const char* filePath) {
  shouldPlayNow = true;
  startPlaying(filePath);
}

void AudioPlayer::playNext(const char* filePath) {
  audioQueue.push(String(filePath));
}

void AudioPlayer::playSkitNext(const ParsedSkit& skit) {
    m_currentSkit = skit;
    m_isPlayingSkit = true;
    m_currentSkitLine = 0;
    m_skitStartTime = 0;  // Will be set when audio actually starts
    playNext(skit.audioFile.c_str());
}

bool AudioPlayer::isCurrentlyPlaying() {
  return isPlaying;
}

bool AudioPlayer::isPlayingSkit() const {
    return m_isPlayingSkit;
}

const ParsedSkit& AudioPlayer::getCurrentSkit() const {
    return m_currentSkit;
}

size_t AudioPlayer::getCurrentSkitLine() const {
    return m_currentSkitLine;
}

bool AudioPlayer::hasStartedPlaying() const {
    return m_hasStartedPlaying;
}

bool AudioPlayer::isBluetoothConnected() const {
    return m_isBluetoothConnected;
}

void AudioPlayer::setBluetoothConnected(bool connected) {
    m_isBluetoothConnected = connected;
}

void AudioPlayer::setAudioReadyToPlay(bool ready) {
    m_isAudioReadyToPlay = ready;
}

int32_t AudioPlayer::provideAudioFrames(Frame* frame, int32_t frame_count) {
    if (audioPlayer) {
        return audioPlayer->_provideAudioFrames(frame, frame_count);
    }
    return 0;
}

// Add detailed debug prints in critical sections
void AudioPlayer::startPlaying(const char* filePath) {
  Serial.printf("startPlaying: Attempting to play file: %s\n", filePath);
  if (currentAudioFile && strcmp(currentAudioFile.name(), filePath) == 0) {
    Serial.println("startPlaying: File is already playing.");
    return;
  }
  if (currentAudioFile) {
    currentAudioFile.close();
  }
  currentAudioFile = FileManager::openFile(filePath);
  isPlaying = currentAudioFile;
  m_isEndOfFile = false;
  m_reachedEndOfFile = false;  // Reset this flag when starting a new file
  m_totalFileSize = currentAudioFile.size();
  m_totalBytesRead = 0;
  m_hasStartedPlaying = true;  // Set this flag when starting playback
  if (!isPlaying) {
    Serial.printf("startPlaying: Failed to open file: %s\n", filePath);
  } else {
    Serial.printf("startPlaying: Successfully opened file: %s (size: %d bytes)\n", filePath, m_totalFileSize);
  }
  logState();  // Log state after starting playback
}

int32_t AudioPlayer::_provideAudioFrames(Frame* frame, int32_t frame_count) {
  if (!isPlaying || !currentAudioFile || m_isEndOfFile) {
    // If playback has ended, check if there's another file in the queue
    if (!audioQueue.empty()) {
      const char* nextAudioFile = audioQueue.front().c_str();
      startPlaying(nextAudioFile);
      audioQueue.pop();
      // If we've successfully started a new file, continue with playback
      if (isPlaying) {
        return this->_provideAudioFrames(frame, frame_count); // Recursive call to handle the new file
      }
    }
    
    // If there's no next file or we couldn't start playing, return silence
    memset(frame, 0, frame_count * sizeof(Frame));
    return frame_count; // Return the requested number of frames, but filled with silence
  }

  size_t requiredSize = frame_count * 4;
  if (requiredSize != currentBufferSize) {
    delete[] buffer;
    buffer = new uint8_t[requiredSize];
    if (!buffer) {
      Serial.println("_provideAudioFrames: Failed to allocate buffer!");
      return 0;
    }
    currentBufferSize = requiredSize;
    Serial.printf("_provideAudioFrames: Buffer resized to %d bytes\n", currentBufferSize);
  }

  size_t bytesRead = FileManager::readFileBytes(currentAudioFile, buffer, currentBufferSize);
  if (bytesRead == 0) {
    Serial.println("_provideAudioFrames: Failed to read from file!");
    return 0;
  }
  m_totalBytesRead += bytesRead;

  // Log progress every 1MB of data read
  if (m_totalBytesRead / 1000000 > (m_totalBytesRead - bytesRead) / 1000000) {
    Serial.printf("_provideAudioFrames: Total read: %d / %d MB\n", m_totalBytesRead / 1000000, m_totalFileSize / 1000000);
  }

  if (bytesRead < currentBufferSize || m_totalBytesRead >= m_totalFileSize) {
    m_isEndOfFile = true;
    isPlaying = false;
    currentAudioFile.close();
    Serial.printf("_provideAudioFrames: Reached end of audio file. Total bytes read: %d / %d\n", m_totalBytesRead, m_totalFileSize);
    return 0;
  }

  // Remove the frame counter and logging here

  for (int i = 0, j = 0; i < bytesRead; i += 4, ++j) {
    frame[j].channel1 = (buffer[i + 1] << 8) | buffer[i];
    frame[j].channel2 = (buffer[i + 3] << 8) | buffer[i + 2];
  }

  return bytesRead / 4;
}

bool AudioPlayer::hasFinishedPlaying() {
  return m_isEndOfFile || !currentAudioFile || currentAudioFile.available() == 0;
}

void AudioPlayer::loadAudioFile(const char* filename) {
    // Implement audio file loading logic here
}

void AudioPlayer::processSkitLine() {
    // Implement skit line processing logic here
}

double AudioPlayer::calculateRMS(const int16_t* samples, int numSamples) {
    double sum = 0;
    for (int i = 0; i < numSamples; i++) {
        sum += samples[i] * samples[i];
    }
    return sqrt(sum / numSamples);
}

void AudioPlayer::performFFT() {
    // Assuming the buffer is filled with samples
    for (int i = 0; i < SAMPLES; i++) {
        // Combine two 8-bit values into a 16-bit sample
        int16_t sample = (int16_t)((buffer[i * 2 + 1] << 8) | buffer[i * 2]);
        vReal[i] = (double)sample;
        vImag[i] = 0;
    }

    FFT.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.Compute(FFT_FORWARD);
    FFT.ComplexToMagnitude();
}

double AudioPlayer::getFFTResult(int index) {
    if (index >= 0 && index < SAMPLES / 2) {
        return vReal[index];
    }
    return 0;
}

ParsedSkit AudioPlayer::parseSkitFile(const String& wavFile, const String& txtFile) {
    return FileManager::parseSkitFile(wavFile, txtFile);
}

ParsedSkit AudioPlayer::findSkitByName(const std::vector<ParsedSkit>& skits, const String& name) {
    return FileManager::findSkitByName(skits, name);
}

bool AudioPlayer::fileExists(fs::FS& fs, const char* path) {
    return FileManager::fileExists(path);
}

void AudioPlayer::setJawPosition(int position) {
    m_servoController.setPosition(position);
}

void AudioPlayer::logState() {
  Serial.println("AudioPlayer State:");
  Serial.printf("  isPlaying: %d\n", isPlaying);
  Serial.printf("  m_isEndOfFile: %d\n", m_isEndOfFile);
  Serial.printf("  m_totalBytesRead: %d\n", m_totalBytesRead);
  Serial.printf("  m_totalFileSize: %d\n", m_totalFileSize);
  Serial.printf("  currentBufferSize: %d\n", currentBufferSize);
  Serial.printf("  audioQueue size: %d\n", audioQueue.size());
  Serial.printf("  m_isPlayingSkit: %d\n", m_isPlayingSkit);
  Serial.printf("  m_currentSkitLine: %d\n", m_currentSkitLine);
}