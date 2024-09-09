#include "skull_audio_animator.h"
#include <cmath>

SkullAudioAnimator::SkullAudioAnimator(ServoController& servoController)
    : m_audioPlayer(), m_servoController(servoController),
      m_isPlayingSkit(false), m_currentSkitLine(0), m_skitStartTime(0),
      FFT(vReal, vImag, SAMPLES, SAMPLE_RATE) {
    // Initialize FFT-related variables
    for (uint16_t i = 0; i < SAMPLES; i++) {
        vReal[i] = 0;
        vImag[i] = 0;
    }

    m_audioPlayer.setBluetoothCallback([this](Frame* frame, int32_t frame_count) {
        return this->provideAudioFrames(frame, frame_count);
    });
    m_audioPlayer.begin();  // Make sure to call begin() here
}

void SkullAudioAnimator::begin() {
    m_audioPlayer.begin();
}

void SkullAudioAnimator::update() {
    m_audioPlayer.update();  // Make sure this method is called
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

void SkullAudioAnimator::updateSkit() {
    if (m_isPlayingSkit && m_audioPlayer.isCurrentlyPlaying()) {
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

void SkullAudioAnimator::processSkitLine() {
    // Implement skit line processing logic here
    // This could involve setting LED states, triggering animations, etc.
}

void SkullAudioAnimator::playNow(const char* filePath) {
    m_audioPlayer.setBluetoothConnected(false);  // Reset connection state
    m_audioPlayer.playNow(filePath);
}

void SkullAudioAnimator::playNext(const char* filePath) {
    m_audioPlayer.playNext(filePath);
}

void SkullAudioAnimator::playSkitNext(const ParsedSkit& skit) {
    m_currentSkit = skit;
    m_isPlayingSkit = true;
    m_currentSkitLine = 0;
    m_skitStartTime = 0;
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
    m_audioPlayer.logState();
    // Add any additional SAA-specific logging here
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
        // Fill the rest with silence if we've reached the end of the file
        memset((uint8_t*)frame + bytesRead, 0, bytesToRead - bytesRead);
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