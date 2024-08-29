#ifndef LIGHT_CONTROLLER_H
#define LIGHT_CONTROLLER_H

#include <Arduino.h>

class LightController {
public:
    LightController(int leftEyePin, int rightEyePin);
    void begin();
    void setEyeBrightness(int brightness);
    void blinkEyes(int numBlinks, int onBrightness = 255, int offBrightness = 0);
    // Removed turnOnEyes() and turnOffEyes()

private:
    int _leftEyePin;
    int _rightEyePin;
    const int PWM_CHANNEL_LEFT = 0;
    const int PWM_CHANNEL_RIGHT = 1;
    const int PWM_FREQUENCY = 5000;  // 5 kHz
    const int PWM_RESOLUTION = 8;    // 8-bit resolution, 0-255
    const int PWM_MAX = 255;         // Maximum PWM value
    void setEyes(int brightness);
};

#endif // LIGHT_CONTROLLER_H