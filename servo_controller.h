#ifndef SERVO_CONTROLLER_H
#define SERVO_CONTROLLER_H

#include <Arduino.h>
#include <Servo.h>

class ServoController {
public:
    ServoController();
    void initialize(int pin, int minDegrees, int maxDegrees);
    void setPosition(int degrees);
    int getPosition() const;
    void setMinMaxDegrees(int minDegrees, int maxDegrees);
    int mapRMSToPosition(double rms, double silenceThreshold);
    void updatePosition(int targetPosition, double alpha, int minMovementThreshold);

private:
    Servo servo;
    int servoPin;
    int currentPosition;
    int minDegrees;
    int maxDegrees;
    double smoothedPosition;
    int lastPosition;
    double maxObservedRMS;
};

#endif // SERVO_CONTROLLER_H