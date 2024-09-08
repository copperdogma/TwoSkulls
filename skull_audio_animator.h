#ifndef SKULL_AUDIO_ANIMATOR_H
#define SKULL_AUDIO_ANIMATOR_H

#include "audio_player.h"
#include "servo_controller.h"
#include <vector>

class SkullAudioAnimator {
public:
    SkullAudioAnimator(AudioPlayer& audioPlayer, ServoController& servoController);

    void begin();
    void update();
    void playNow(const char* filePath);
    void playNext(const char* filePath);
    void playSkitNext(const ParsedSkit& skit);
    bool isCurrentlyPlaying();
    bool isPlayingSkit() const;
    bool hasFinishedPlaying();
    void setBluetoothConnected(bool connected);
    void setAudioReadyToPlay(bool ready);
    ParsedSkit findSkitByName(const std::vector<ParsedSkit>& skits, const String& name);
    size_t getTotalBytesRead() const;
    void logState();

private:
    AudioPlayer& m_audioPlayer;
    ServoController& m_servoController;
};

#endif // SKULL_AUDIO_ANIMATOR_H