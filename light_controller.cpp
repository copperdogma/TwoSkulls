#include "light_controller.h"

LightController::LightController(int leftEyePin, int rightEyePin)
    : _leftEyePin(leftEyePin), _rightEyePin(rightEyePin), _currentBrightness(BRIGHTNESS_OFF) {}

void LightController::begin()
{
    pinMode(_leftEyePin, OUTPUT);
    pinMode(_rightEyePin, OUTPUT);

    ledcSetup(PWM_CHANNEL_LEFT, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcSetup(PWM_CHANNEL_RIGHT, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcAttachPin(_leftEyePin, PWM_CHANNEL_LEFT);
    ledcAttachPin(_rightEyePin, PWM_CHANNEL_RIGHT);

    setEyeBrightness(BRIGHTNESS_MAX);
}

void LightController::setEyeBrightness(uint8_t brightness)  // Changed int to uint8_t
{
    // Clamp the values to be between 0 and 255 (min and max)
    brightness = max(BRIGHTNESS_OFF, min(brightness, BRIGHTNESS_MAX));

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
        delay(100); // On for 100ms
        setEyeBrightness(offBrightness);
        delay(100); // Off for 100ms
    }
    setEyeBrightness(onBrightness); // End with eyes on
}