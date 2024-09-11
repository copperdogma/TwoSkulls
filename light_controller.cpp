#include "light_controller.h"

LightController::LightController(int leftEyePin, int rightEyePin)
    : _leftEyePin(leftEyePin), _rightEyePin(rightEyePin), _currentBrightness(0) {}

void LightController::begin()
{
    pinMode(_leftEyePin, OUTPUT);
    pinMode(_rightEyePin, OUTPUT);

    ledcSetup(PWM_CHANNEL_LEFT, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcSetup(PWM_CHANNEL_RIGHT, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcAttachPin(_leftEyePin, PWM_CHANNEL_LEFT);
    ledcAttachPin(_rightEyePin, PWM_CHANNEL_RIGHT);

    setEyeBrightness(PWM_MAX);
}

void LightController::setEyeBrightness(int brightness)
{
    // Clamp the values to be between 0 and 255
    brightness = max(0, min(brightness, 255));

    // If the current brightness is different from the new brightness, update the LEDs
    if (brightness != _currentBrightness) {
        ledcWrite(PWM_CHANNEL_LEFT, brightness);
        ledcWrite(PWM_CHANNEL_RIGHT, brightness);
        _currentBrightness = brightness;
        Serial.println("Updated eye brightness to: " + String(brightness));
    }
}

void LightController::blinkEyes(int numBlinks, int onBrightness, int offBrightness)
{
    for (int i = 0; i < numBlinks; i++)
    {
        setEyeBrightness(onBrightness);
        delay(200); // On for 200ms
        setEyeBrightness(offBrightness);
        delay(200); // Off for 200ms
    }
    setEyeBrightness(onBrightness); // End with eyes on
}