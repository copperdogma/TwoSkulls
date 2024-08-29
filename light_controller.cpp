#include "light_controller.h"

LightController::LightController(int leftEyePin, int rightEyePin)
    : _leftEyePin(leftEyePin), _rightEyePin(rightEyePin) {}

void LightController::begin() {
    pinMode(_leftEyePin, OUTPUT);
    pinMode(_rightEyePin, OUTPUT);

    ledcSetup(PWM_CHANNEL_LEFT, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcSetup(PWM_CHANNEL_RIGHT, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcAttachPin(_leftEyePin, PWM_CHANNEL_LEFT);
    ledcAttachPin(_rightEyePin, PWM_CHANNEL_RIGHT);

    // Instead of turnOnEyes(), use setEyes() with full brightness
    setEyes(PWM_MAX);
}

void LightController::setEyeBrightness(int brightness) {
    setEyes(brightness);
}

void LightController::setEyes(int brightness) {
    ledcWrite(PWM_CHANNEL_LEFT, brightness);
    ledcWrite(PWM_CHANNEL_RIGHT, brightness);
}

void LightController::blinkEyes(int numBlinks, int onBrightness, int offBrightness) {
    for (int i = 0; i < numBlinks; i++) {
        setEyes(onBrightness);
        delay(200);  // On for 200ms
        setEyes(offBrightness);
        delay(200);  // Off for 200ms
    }
    setEyes(onBrightness);  // End with eyes on
}

// Remove turnOnEyes() and turnOffEyes() methods