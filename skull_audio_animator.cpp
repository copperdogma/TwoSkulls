#include "skull_audio_animator.h"
#include <cmath>

SkullAudioAnimator::SkullAudioAnimator(ServoController& servoController, LightController& lightController)
    : m_audioPlayer(), m_servoController(servoController), m_lightController(lightController), 
      m_isPlayingSkit(false), m_currentSkitLine(0),
      FFT(vReal, vImag, SAMPLES, SAMPLE_RATE) {
    // ... (keep the rest of the constructor)
}

void SkullAudioAnimator::begin() {
    m_audioPlayer.begin();
}

void SkullAudioAnimator::update() {
    m_audioPlayer.update();
    updateEyes();
    updateJawPosition();
    updateSkit();
}

void SkullAudioAnimator::updateJawPosition() {
    if (m_audioPlayer.isCurrentlyPlaying()) {
        performFFT();
        double bassEnergy = 0;
        for (int i = 1; i < 10; i++) {
            bassEnergy += getFFTResult(i);
        }
        int jawPosition = map(bassEnergy, 0, 1000, 0, 90);
        m_servoController.setPosition(jawPosition);
    }
}

void SkullAudioAnimator::updateEyes() {
    if (!m_audioPlayer.isCurrentlyPlaying()) {
        // Reset eyes to dim when audio is not playing
        m_lightController.setEyeBrightness(LightController::BRIGHTNESS_DIM);
        return;
    }

    if (!m_currentSkit.lines.empty()) {
        unsigned long playbackTime = m_audioPlayer.getPlaybackTime();

        // Find the current skit line based on playback time
        while (m_currentSkitLine < m_currentSkit.lines.size() &&
               m_currentSkit.lines[m_currentSkitLine].timestamp <= playbackTime) {
            m_currentSkitLine++;
        }
        
        if (m_currentSkitLine > 0) {
            m_currentSkitLine--;  // Adjust to get the current (not next) line
        }

        ParsedSkitLine currentLine = m_currentSkit.lines[m_currentSkitLine];

        // Update eye brightness based on speaker
        if (currentLine.speaker == 'A') {
            m_lightController.setEyeBrightness(LightController::BRIGHTNESS_MAX);
        } else {
            m_lightController.setEyeBrightness(LightController::BRIGHTNESS_DIM);
        }
    }
    else {
        Serial.println("SkullAudioAnimator: Playing audio, but no current skit lines");
    }
}

void SkullAudioAnimator::updateSkit() {
    if (m_isPlayingSkit && m_audioPlayer.isCurrentlyPlaying()) {
        unsigned long currentTime = m_audioPlayer.getPlaybackTime();

        while (m_currentSkitLine < m_currentSkit.lines.size() && 
               currentTime >= m_currentSkit.lines[m_currentSkitLine].timestamp) {
            m_currentSkitLine++;
        }

        if (m_currentSkitLine >= m_currentSkit.lines.size()) {
            m_isPlayingSkit = false;
        }
    }
}

void SkullAudioAnimator::playNow(const char* filePath) {
    m_audioPlayer.setBluetoothConnected(false);  // Reset connection state
    m_audioPlayer.playNow(filePath);
}

void SkullAudioAnimator::playNext(const char* filePath) {
    // Kept this log as it's useful for tracking queue operations
    Serial.printf("SkullAudioAnimator: Queuing next audio file: %s\n", filePath);
    m_audioPlayer.playNext(filePath);
}

void SkullAudioAnimator::playSkitNext(const ParsedSkit& skit) {
    m_currentSkit = skit;
    m_isPlayingSkit = true;
    m_currentSkitLine = 0;
    m_audioPlayer.playNext(skit.audioFile.c_str());
}

bool SkullAudioAnimator::isCurrentlyPlaying() {
    return m_audioPlayer.isCurrentlyPlaying();
}

bool SkullAudioAnimator::isPlayingSkit() const {
    return m_isPlayingSkit;
}

bool SkullAudioAnimator::hasFinishedPlaying() {
    return m_audioPlayer.hasFinishedPlaying();
}

void SkullAudioAnimator::setBluetoothConnected(bool connected) {
    m_audioPlayer.setBluetoothConnected(connected);
}

void SkullAudioAnimator::setAudioReadyToPlay(bool ready) {
    m_audioPlayer.setAudioReadyToPlay(ready);
}

ParsedSkit SkullAudioAnimator::findSkitByName(const std::vector<ParsedSkit>& skits, const String& name) {
    for (const auto& skit : skits) {
        if (skit.audioFile == name) {  // Changed from skit.name to skit.audioFile
            return skit;
        }
    }
    return ParsedSkit(); // Return an empty ParsedSkit if not found
}

size_t SkullAudioAnimator::getTotalBytesRead() const {
    return m_audioPlayer.getTotalBytesRead();
}

void SkullAudioAnimator::logState() {
    Serial.println("SkullAudioAnimator State:");
    Serial.printf("  Is playing skit: %d\n", m_isPlayingSkit);
    Serial.printf("  Current skit line: %d\n", m_currentSkitLine);
    // Remove the line that printed m_skitStartTime
}

bool SkullAudioAnimator::fileExists(fs::FS &fs, const char* path) {
    return m_audioPlayer.fileExists(fs, path);
}

int32_t SkullAudioAnimator::provideAudioFrames(Frame* frame, int32_t frame_count) {
    if (!m_audioPlayer.isCurrentlyPlaying()) {
        memset(frame, 0, frame_count * sizeof(Frame));
        return frame_count;
    }

    size_t bytesToRead = frame_count * sizeof(Frame);
    size_t bytesRead = m_audioPlayer.readAudioData((uint8_t*)frame, bytesToRead);
    
    if (bytesRead < bytesToRead) {
        memset((uint8_t*)frame + bytesRead, 0, bytesToRead - bytesRead);
        
        if (bytesRead == 0) {
            // Kept this log as it's useful for tracking playback status
            Serial.println("SkullAudioAnimator: End of file reached, attempting to play next file");
            m_audioPlayer.playNext(nullptr);
        }
    }
    
    m_audioPlayer.incrementTotalBytesRead(bytesRead);
    return frame_count;
}

ParsedSkit SkullAudioAnimator::parseSkitFile(const String& wavFile, const String& txtFile) {
    // Implement skit file parsing logic
    // This is a placeholder implementation. You should replace it with actual parsing logic.
    ParsedSkit skit;
    skit.audioFile = wavFile;
    skit.txtFile = txtFile;
    // Parse the txt file and populate skit.lines
    return skit;
}

const ParsedSkit& SkullAudioAnimator::getCurrentSkit() const {
    return m_currentSkit;
}

void SkullAudioAnimator::performFFT() {
    // Get the current audio buffer
    Frame buffer[SAMPLES];
    int32_t bufferSize = m_audioPlayer.provideAudioFrames(buffer, SAMPLES);

    // Fill vReal with audio samples
    for (uint16_t i = 0; i < SAMPLES; i++) {
        if (i < bufferSize) {
            vReal[i] = (double)buffer[i].channel1;  // Use channel1 or average of both channels
        } else {
            vReal[i] = 0;
        }
        vImag[i] = 0;
    }

    // Perform FFT
    FFT.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.Compute(FFT_FORWARD);
    FFT.ComplexToMagnitude();
}

double SkullAudioAnimator::getFFTResult(int index) {
    if (index >= 0 && index < SAMPLES / 2) {
        return vReal[index];
    }
    return 0;
}

double SkullAudioAnimator::calculateRMS(const int16_t* samples, int numSamples) {
    double sum = 0;
    for (int i = 0; i < numSamples; i++) {
        sum += samples[i] * samples[i];
    }
    return sqrt(sum / numSamples);
}