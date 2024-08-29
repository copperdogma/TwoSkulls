#ifndef SERVO_CONTROLLER_H
#define SERVO_CONTROLLER_H

#include <Servo.h> // This complains; I'm actually using the local ESP32_ESP32S2_AnalogWrite library to support the ESP32

class ServoController {
public:
    ServoController();
    void initialize(int pin, int minDegrees, int maxDegrees);
    void setPosition(int position);
    int mapRMSToPosition(double rms, double silenceThreshold);
    void updatePosition(int targetPosition, double alpha, int minMovementThreshold);

private:
    Servo jawServo;
    int servoPin;
    int servoMinDegrees;
    int servoMaxDegrees;
    double smoothedPosition;
    int lastPosition;
    double maxObservedRMS;
};

#endif // SERVO_CONTROLLER_H