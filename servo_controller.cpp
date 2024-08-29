#include "servo_controller.h"
#include <algorithm>
#include <cmath>

ServoController::ServoController()
    : servoPin(-1), currentPosition(0), minDegrees(0), maxDegrees(0),
      smoothedPosition(0), lastPosition(0), maxObservedRMS(0) {}

void ServoController::initialize(int pin, int minDeg, int maxDeg) {
    servoPin = pin;
    servo.attach(servoPin);
    minDegrees = minDeg;
    maxDegrees = maxDeg;
    currentPosition = minDeg;
    servo.write(servoPin, currentPosition);
}

void ServoController::setPosition(int degrees) {
    int constrainedDegrees = constrain(degrees, minDegrees, maxDegrees);
    servo.write(servoPin, constrainedDegrees);
    currentPosition = constrainedDegrees;
}

int ServoController::mapRMSToPosition(double rms, double silenceThreshold) {
    if (rms < silenceThreshold) {
        return minDegrees;
    }

    if (rms > maxObservedRMS) {
        maxObservedRMS = rms;
    }

    double normalizedRMS = std::min(rms / maxObservedRMS, 1.0);
    double mappedValue = pow(normalizedRMS, 0.2);  // 0.2 is the MOVE_EXPONENT

    int minJawOpening = minDegrees + 5;  // 5 degrees minimum opening

    return map(mappedValue * 1000, 0, 1000, minJawOpening, maxDegrees);
}

void ServoController::updatePosition(int targetPosition, double alpha, int minMovementThreshold) {
    smoothedPosition = alpha * targetPosition + (1 - alpha) * smoothedPosition;
    int newPosition = round(smoothedPosition);
    newPosition = constrain(newPosition, minDegrees, maxDegrees);

    if (abs(newPosition - lastPosition) > minMovementThreshold) {
        setPosition(newPosition);
        lastPosition = newPosition;
    }
}