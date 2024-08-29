#include "skull_audio_animator.h"

const double SkullAudioAnimator::ALPHA = 0.2;
const double SkullAudioAnimator::SILENCE_THRESHOLD = 200.0;
const int SkullAudioAnimator::MIN_MOVEMENT_THRESHOLD = 3;
const int SkullAudioAnimator::SERVO_MIN_DEGREES = 0;
const int SkullAudioAnimator::SERVO_MAX_DEGREES = 80;

SkullAudioAnimator::SkullAudioAnimator(AudioPlayer* audioPlayer, ServoController& servoController, bool isPrimary)
    : audioPlayer(audioPlayer), servoController(servoController), isPrimary(isPrimary) {}

void SkullAudioAnimator::update() {
    if (audioPlayer->isCurrentlyPlaying()) {
        processAudio();
    } else {
        servoController.setPosition(SERVO_MIN_DEGREES);
    }
}

void SkullAudioAnimator::processAudio() {
    const uint8_t* audioBuffer = audioPlayer->getCurrentAudioBuffer();
    size_t audioBufferSize = audioPlayer->getCurrentAudioBufferSize();

    for (size_t offset = 0; offset < audioBufferSize; offset += AudioPlayer::SAMPLES * 2) {
        std::vector<int16_t> samples(AudioPlayer::SAMPLES);
        for (size_t i = 0; i < AudioPlayer::SAMPLES && (offset + i * 2 + 1) < audioBufferSize; ++i) {
            samples[i] = (audioBuffer[offset + i * 2 + 1] << 8) | audioBuffer[offset + i * 2];
        }

        double rms = audioPlayer->calculateRMS(samples.data(), samples.size());
        audioPlayer->performFFT();

        updateJawPosition(rms);
    }
}

void SkullAudioAnimator::updateJawPosition(double rms) {
    int targetPosition = servoController.mapRMSToPosition(rms, SILENCE_THRESHOLD);
    
    if (abs(targetPosition - servoController.getCurrentPosition()) >= MIN_MOVEMENT_THRESHOLD) {
        servoController.setPosition(targetPosition);
    }
}

int SkullAudioAnimator::mapRMSToPosition(double rms, double silenceThreshold) {
    if (rms < silenceThreshold) {
        return SERVO_MIN_DEGREES;
    }
    double normalizedRMS = (rms - silenceThreshold) / (32768.0 - silenceThreshold);
    return map(pow(normalizedRMS, 0.2) * 100, 0, 100, SERVO_MIN_DEGREES, SERVO_MAX_DEGREES);
}