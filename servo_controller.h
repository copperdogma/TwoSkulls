#ifndef SERVO_CONTROLLER_H
#define SERVO_CONTROLLER_H

#include <Servo.h>

class ServoController {
private:
    Servo servo;
    int servoPin;
    int currentPosition;
    int minDegrees;
    int maxDegrees;
    double smoothedPosition;
    int lastPosition;
    double maxObservedRMS;

public:
    ServoController();  // Add constructor declaration
    void initialize(int pin, int minDeg, int maxDeg);
    void setPosition(int degrees);
    int getCurrentPosition() const { return currentPosition; }
    int mapRMSToPosition(double rms, double silenceThreshold);
    void updatePosition(int targetPosition, double alpha, int minMovementThreshold);
};

#endif // SERVO_CONTROLLER_H