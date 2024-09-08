#include "skull_audio_animator.h"
#include <cmath>

SkullAudioAnimator::SkullAudioAnimator(AudioPlayer& audioPlayer, ServoController& servoController)
    : m_audioPlayer(audioPlayer), m_servoController(servoController),
      FFT(vReal, vImag, SAMPLES, SAMPLE_RATE) {
    // Initialize FFT-related variables
    for (uint16_t i = 0; i < SAMPLES; i++) {
        vReal[i] = 0;
        vImag[i] = 0;
    }
}

void SkullAudioAnimator::begin() {
    m_audioPlayer.begin();
}

void SkullAudioAnimator::update() {
    m_audioPlayer.update();

    if (m_audioPlayer.isCurrentlyPlaying()) {
        // Perform FFT analysis
        performFFT();
        
        // Use FFT results for animation (e.g., jaw movement)
        double bassEnergy = 0;
        for (int i = 1; i < 10; i++) {  // Analyze first 10 frequency bins (adjust as needed)
            bassEnergy += getFFTResult(i);
        }
        
        // Map bass energy to jaw position (adjust thresholds as needed)
        int jawPosition = map(bassEnergy, 0, 1000, 0, 90);
        m_servoController.setPosition(jawPosition);
    }
}

void SkullAudioAnimator::playNow(const char* filePath) {
    m_audioPlayer.playNow(filePath);
}

void SkullAudioAnimator::playNext(const char* filePath) {
    m_audioPlayer.playNext(filePath);
}

void SkullAudioAnimator::playSkitNext(const ParsedSkit& skit) {
    m_audioPlayer.playSkitNext(skit);
}

bool SkullAudioAnimator::isCurrentlyPlaying() {
    return m_audioPlayer.isCurrentlyPlaying();
}

bool SkullAudioAnimator::isPlayingSkit() const {
    return m_audioPlayer.isPlayingSkit();
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
    return m_audioPlayer.findSkitByName(skits, name);
}

size_t SkullAudioAnimator::getTotalBytesRead() const {
    return m_audioPlayer.getTotalBytesRead();
}

void SkullAudioAnimator::logState() {
    m_audioPlayer.logState();
}

// Remove this entire block
// void SkullAudioAnimator::loadAudioFile(const char* filename) {
//     m_audioPlayer.loadAudioFile(filename);
// }

// Update this method to use the AudioPlayer's fileExists method
bool SkullAudioAnimator::fileExists(fs::FS &fs, const char* path) {
    return m_audioPlayer.fileExists(fs, path);
}

// Review and adjust these methods if necessary
double SkullAudioAnimator::calculateRMS(const int16_t* samples, int numSamples) {
    double sum = 0;
    for (int i = 0; i < numSamples; i++) {
        sum += samples[i] * samples[i];
    }
    return sqrt(sum / numSamples);
}

void SkullAudioAnimator::performFFT() {
    // Get the current audio buffer
    uint8_t* buffer = m_audioPlayer.getCurrentAudioBuffer();
    size_t bufferSize = m_audioPlayer.getCurrentAudioBufferSize();

    // Fill vReal with audio samples
    for (uint16_t i = 0; i < SAMPLES; i++) {
        if (i * 2 + 1 < bufferSize) {
            int16_t sample = (buffer[i * 2 + 1] << 8) | buffer[i * 2];
            vReal[i] = (double)sample;
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

int32_t SkullAudioAnimator::provideAudioFrames(Frame* frame, int32_t frame_count) {
    return m_audioPlayer.provideAudioFrames(frame, frame_count);
}

ParsedSkit SkullAudioAnimator::parseSkitFile(const String& wavFile, const String& txtFile) {
    return m_audioPlayer.parseSkitFile(wavFile, txtFile);
}