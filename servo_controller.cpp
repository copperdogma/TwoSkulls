#include "servo_controller.h"
#include <algorithm>
#include <cmath>

ServoController::ServoController()
    : servoPin(0), servoMinDegrees(0), servoMaxDegrees(0),
      smoothedPosition(0), lastPosition(0), maxObservedRMS(0) {}

void ServoController::initialize(int pin, int minDegrees, int maxDegrees) {
    servoPin = pin;
    servoMinDegrees = minDegrees;
    servoMaxDegrees = maxDegrees;

    Serial.printf("Initializing servo on pin %d (min: %d, max: %d)\n", servoPin, servoMinDegrees, servoMaxDegrees);
    jawServo.attach(servoPin);

    Serial.printf("Servo animation init: %d (min) degrees\n", servoMinDegrees);
    setPosition(servoMinDegrees);
    Serial.printf("Servo animation init: %d (max) degrees\n", servoMaxDegrees);
    delay(500);
    setPosition(servoMaxDegrees);
    Serial.println("Servo animation init complete; resetting to 0 degrees");
    delay(500);
    setPosition(servoMinDegrees);
}

void ServoController::setPosition(int position) {
    if (position > servoMaxDegrees) { 
        Serial.printf("WARNING: requested jaw position %d is greater than max degrees %d\n", position, servoMaxDegrees);
        position = servoMaxDegrees;
    }

    jawServo.write(servoPin, position);
}

int ServoController::mapRMSToPosition(double rms, double silenceThreshold) {
    if (rms < silenceThreshold) {
        return servoMinDegrees;
    }

    if (rms > maxObservedRMS) {
        maxObservedRMS = rms;
    }

    double normalizedRMS = std::min(rms / maxObservedRMS, 1.0);
    double mappedValue = pow(normalizedRMS, 0.2);  // 0.2 is the MOVE_EXPONENT

    int minJawOpening = servoMinDegrees + 5;  // 5 degrees minimum opening

    return map(mappedValue * 1000, 0, 1000, minJawOpening, servoMaxDegrees);
}

void ServoController::updatePosition(int targetPosition, double alpha, int minMovementThreshold) {
    smoothedPosition = alpha * targetPosition + (1 - alpha) * smoothedPosition;
    int newPosition = round(smoothedPosition);
    newPosition = constrain(newPosition, servoMinDegrees, servoMaxDegrees);

    if (abs(newPosition - lastPosition) > minMovementThreshold) {
        setPosition(newPosition);
        lastPosition = newPosition;
    }
}