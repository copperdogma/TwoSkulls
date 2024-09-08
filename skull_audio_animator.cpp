#include "skull_audio_animator.h"

SkullAudioAnimator::SkullAudioAnimator(AudioPlayer& audioPlayer, ServoController& servoController)
    : m_audioPlayer(audioPlayer), m_servoController(servoController) {}

void SkullAudioAnimator::begin() {
    m_audioPlayer.begin();
}

void SkullAudioAnimator::update() {
    m_audioPlayer.update();
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
    return m_audioPlayer.calculateRMS(samples, numSamples);
}

void SkullAudioAnimator::performFFT() {
    m_audioPlayer.performFFT();
}

double SkullAudioAnimator::getFFTResult(int index) {
    return m_audioPlayer.getFFTResult(index);
}

int32_t SkullAudioAnimator::provideAudioFrames(Frame* frame, int32_t frame_count) {
    return m_audioPlayer.provideAudioFrames(frame, frame_count);
}

ParsedSkit SkullAudioAnimator::parseSkitFile(const String& wavFile, const String& txtFile) {
    return m_audioPlayer.parseSkitFile(wavFile, txtFile);
}