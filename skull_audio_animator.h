#ifndef SKULL_AUDIO_ANIMATOR_H
#define SKULL_AUDIO_ANIMATOR_H

#include <Arduino.h>
#include "audio_player.h"
#include "servo_controller.h"

class SkullAudioAnimator {
public:
    SkullAudioAnimator(AudioPlayer* audioPlayer, ServoController& servoController, bool isPrimary);
    void update();

private:
    AudioPlayer* audioPlayer;
    ServoController& servoController;
    bool isPrimary;

    void processAudio();
    void updateJawPosition(double rms);
    int mapRMSToPosition(double rms, double silenceThreshold);

    static const double ALPHA;
    static const double SILENCE_THRESHOLD;
    static const int MIN_MOVEMENT_THRESHOLD;
    static const int SERVO_MIN_DEGREES;
    static const int SERVO_MAX_DEGREES;
};

#endif // SKULL_AUDIO_ANIMATOR_H