#include "audio_player.h"
#include <cmath>
#include <algorithm>

AudioPlayer::AudioPlayer() 
    : FFT(vReal, vImag, SAMPLES, SAMPLE_RATE),
      buffer(nullptr), currentBufferSize(0), shouldPlayNow(false), isPlaying(false),
      m_isPlayingSkit(false), m_currentSkitLine(0), m_skitStartTime(0),
      m_hasStartedPlaying(false), m_isBluetoothConnected(false), m_isAudioReadyToPlay(false),
      smoothedPosition(0), maxObservedRMS(0), lastServoPosition(0),
      servoPin(0), servoMinDegrees(0), servoMaxDegrees(0),
      jawServo() {}  // Initialize jawServo

void AudioPlayer::begin() {
    // Initialize audio player
}

void AudioPlayer::update() {
  if (shouldPlayNow) {
    Serial.printf("AudioPlayer: Now playing file: %s\n", currentAudioFile.name());
    startPlaying(currentAudioFile.name());
    shouldPlayNow = false;
  } else if (hasFinishedPlaying()) {
    if (!audioQueue.empty()) {
      const char* nextAudioFile = audioQueue.front().c_str();
      Serial.printf("AudioPlayer: Playing next queued file: %s\n", nextAudioFile);
      startPlaying(nextAudioFile);
      audioQueue.pop();
    }
  }

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

void AudioPlayer::startPlaying(const char* filePath) {
  if (currentAudioFile && strcmp(currentAudioFile.name(), filePath) == 0) {
    // If the current file is already the desired one, return.
    return;
  }
  currentAudioFile.close();
  currentAudioFile = SD.open(filePath);
  if (!currentAudioFile) {
    Serial.printf("Failed to open file: %s\n", filePath);
  } else {
    isPlaying = true;
  }
}

int32_t AudioPlayer::_provideAudioFrames(Frame* frame, int32_t frame_count) {
    if (!isPlaying) return 0;

    if (!currentAudioFile) {
        Serial.println("_provideAudioFrames: currentAudioFile not ready. Exiting early to await data.");
        return 0;
    }

    size_t requiredSize = frame_count * 2 * 2;  // Stereo, 2 channels
    if (requiredSize != currentBufferSize) {
        delete[] buffer;
        buffer = new uint8_t[requiredSize];
        currentBufferSize = requiredSize;
    }

    size_t bytesRead = currentAudioFile.read(buffer, currentBufferSize);

    if (bytesRead < 1) {
        isPlaying = false;
        return 0;
    }

    isPlaying = true;

    for (int i = 0, j = 0; i < bytesRead; i += 4, ++j) {
        frame[j].channel1 = (buffer[i + 1] << 8) | buffer[i];
        frame[j].channel2 = (buffer[i + 3] << 8) | buffer[i + 2];
    }

    return bytesRead / 4;
}

bool AudioPlayer::hasFinishedPlaying() {
  return !currentAudioFile || currentAudioFile.available() == 0;
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
  ParsedSkit parsedSkit;
  parsedSkit.audioFile = wavFile;
  parsedSkit.txtFile = txtFile;

  File file = SD.open(txtFile);
  if (!file) {
    Serial.println("Failed to open skit file: " + txtFile);
    return parsedSkit;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    ParsedSkitLine skitLine;
    int commaIndex1 = line.indexOf(',');
    int commaIndex2 = line.indexOf(',', commaIndex1 + 1);
    int commaIndex3 = line.indexOf(',', commaIndex2 + 1);

    skitLine.speaker = line.charAt(0);
    skitLine.timestamp = line.substring(commaIndex1 + 1, commaIndex2).toInt();
    skitLine.duration = line.substring(commaIndex2 + 1, commaIndex3).toInt();

    if (commaIndex3 != -1) {
      skitLine.jawPosition = line.substring(commaIndex3 + 1).toFloat();
    } else {
      skitLine.jawPosition = -1;  // Indicating dynamic jaw movement
    }

    parsedSkit.lines.push_back(skitLine);
  }

  file.close();
  return parsedSkit;
}

ParsedSkit AudioPlayer::findSkitByName(const std::vector<ParsedSkit>& skits, const String& name) {
  for (const auto& skit : skits) {
    if (skit.audioFile.endsWith(name + ".wav")) {
      return skit;
    }
  }
  return { "", "", {} };  // Return empty ParsedSkit if not found
}

Skit AudioPlayer::getRandomSkit(const std::vector<Skit>& skits) {
  if (skits.empty()) {
    return Skit();  // Return an empty Skit if the vector is empty
  }
  int randomIndex = random(skits.size());
  return skits[randomIndex];
}

void AudioPlayer::prepareSkitPlayback(const ParsedSkit& skit) {
  audioSegments.clear();

  for (const auto& line : skit.lines) {
    bool shouldPlay = (m_isPlayingSkit && line.speaker == 'A') || (!m_isPlayingSkit && line.speaker == 'B');
    audioSegments.push_back({ line.timestamp, line.duration, shouldPlay });
  }

  // Sort segments by start time
  std::sort(audioSegments.begin(), audioSegments.end(),
            [](const AudioSegment& a, const AudioSegment& b) {
              return a.start < b.start;
            });

  currentSegment = 0;
}

bool AudioPlayer::fileExists(fs::FS& fs, const char* path) {
  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    return false;
  }
  file.close();
  return true;
}

int AudioPlayer::mapRMSToServoPosition(double rms, double silenceThreshold, int servoMinDegrees, int servoMaxDegrees) {
  // Check for silence or very low volume
  if (rms < silenceThreshold) {
    return servoMinDegrees;  // Close the jaw completely
  }

  // Update max observed RMS
  if (rms > maxObservedRMS) {
    maxObservedRMS = rms;
  }

  // Normalize RMS to 0-1 range, using the max observed RMS
  double normalizedRMS = std::min(rms / maxObservedRMS, 1.0);

  // Apply non-linear mapping for more pronounced mouth movement
  double mappedValue = pow(normalizedRMS, 0.2);  // 0.2 is the MOVE_EXPONENT

  // Increase the minimum jaw opening
  int minJawOpening = servoMinDegrees + 5;  // 5 degrees minimum opening

  return map(mappedValue * 1000, 0, 1000, minJawOpening, servoMaxDegrees);
}

void AudioPlayer::updateServoPosition(int targetPosition, int servoMinDegrees, int servoMaxDegrees, double alpha, int minMovementThreshold) {
    smoothedPosition = alpha * targetPosition + (1 - alpha) * smoothedPosition;
    int newPosition = round(smoothedPosition);
    newPosition = constrain(newPosition, servoMinDegrees, servoMaxDegrees);

    if (abs(newPosition - lastServoPosition) > minMovementThreshold) {
        setJawPosition(newPosition);
        lastServoPosition = newPosition;
    }
}

void AudioPlayer::initializeServo(int pin, int minDegrees, int maxDegrees) {
    servoPin = pin;
    servoMinDegrees = minDegrees;
    servoMaxDegrees = maxDegrees;
    Serial.printf("Initializing servo on pin %d (min: %d, max: %d)\n", servoPin, servoMinDegrees, servoMaxDegrees);
    
    // No need for attach() with this library
    setJawPosition(servoMinDegrees);
}

void AudioPlayer::setJawPosition(int position) {
    position = constrain(position, servoMinDegrees, servoMaxDegrees);
    jawServo.write(servoPin, position);
}

void AudioPlayer::handleBluetoothStateChange(esp_a2d_connection_state_t state) {
  m_isBluetoothConnected = (state == ESP_A2D_CONNECTION_STATE_CONNECTED);
  // Additional logic as needed
}

// Remove the following function as it's already defined earlier in the file
// bool AudioPlayer::isBluetoothConnected() const {
//   return m_isBluetoothConnected;
// }