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