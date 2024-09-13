#ifndef SERVO_CONTROLLER_H
#define SERVO_CONTROLLER_H

#include <Servo.h>
#include <atomic>

class ServoController {
private:
    Servo servo;
    int servoPin;
    std::atomic<int> currentPosition;
    int minDegrees;
    int maxDegrees;
    std::atomic<double> smoothedPosition;
    std::atomic<int> lastPosition;
    std::atomic<double> maxObservedRMS;

public:
    ServoController();
    void initialize(int pin, int minDeg, int maxDeg);
    void setPosition(int degrees);
    int getCurrentPosition() const { return currentPosition.load(std::memory_order_relaxed); }
    int mapRMSToPosition(double rms, double silenceThreshold);
    void updatePosition(int targetPosition, double alpha, int minMovementThreshold);
};

#endif // SERVO_CONTROLLER_H