#ifndef LIGHT_CONTROLLER_H
#define LIGHT_CONTROLLER_H

#include <Arduino.h>

#define PWM_FREQUENCY 5000
#define PWM_RESOLUTION 8
#define PWM_MAX 255
#define PWM_CHANNEL_LEFT 0
#define PWM_CHANNEL_RIGHT 1

class LightController {
public:
    LightController(int leftEyePin, int rightEyePin);
    void begin();
    void setEyeBrightness(int brightness);
    void blinkEyes(int numBlinks, int onBrightness = PWM_MAX, int offBrightness = 0);

private:
    int _leftEyePin;
    int _rightEyePin;
    int _currentBrightness;  // Add this line
};

#endif // LIGHT_CONTROLLER_H