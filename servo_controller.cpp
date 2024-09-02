#include "servo_controller.h"
#include <algorithm>
#include <cmath>

ServoController::ServoController()
    : servoPin(-1), currentPosition(0), minDegrees(0), maxDegrees(0),
      smoothedPosition(0), lastPosition(0), maxObservedRMS(0) {}

void ServoController::initialize(int pin, int minDeg, int maxDeg)
{
    servoPin = pin;
    minDegrees = minDeg;
    maxDegrees = maxDeg;
    currentPosition = minDeg;

    Serial.printf("Initializing servo on pin %d (min: %d, max: %d)\n", servoPin, minDegrees, maxDegrees);
    servo.attach(servoPin);
    setPosition(currentPosition);

    Serial.printf("Servo animation init: %d (min) degrees\n", minDegrees);
    setPosition(minDegrees);
    Serial.printf("Servo animation init: %d (max) degrees\n", maxDegrees);
    delay(500);
    setPosition(maxDegrees);
    Serial.println("Servo animation init complete; resetting to 0 degrees");
    delay(500);
    setPosition(minDegrees);
}

void ServoController::setPosition(int degrees)
{
    int constrainedDegrees = constrain(degrees, minDegrees, maxDegrees);
    servo.write(servoPin, constrainedDegrees);
    currentPosition.store(constrainedDegrees, std::memory_order_relaxed);
}

int ServoController::mapRMSToPosition(double rms, double silenceThreshold)
{
    if (rms < silenceThreshold)
    {
        return minDegrees;
    }

    if (rms > maxObservedRMS)
    {
        maxObservedRMS = rms;
    }

    double normalizedRMS = std::min(rms / maxObservedRMS, 1.0);
    double mappedValue = pow(normalizedRMS, 0.2); // 0.2 is the MOVE_EXPONENT

    int minJawOpening = minDegrees + 5; // 5 degrees minimum opening

    return map(mappedValue * 1000, 0, 1000, minJawOpening, maxDegrees);
}

void ServoController::updatePosition(int targetPosition, double alpha, int minMovementThreshold)
{
    double current = smoothedPosition.load(std::memory_order_relaxed);
    double updated = alpha * targetPosition + (1 - alpha) * current;
    smoothedPosition.store(updated, std::memory_order_relaxed);

    int newPosition = round(updated);
    newPosition = constrain(newPosition, minDegrees, maxDegrees);

    int last = lastPosition.load(std::memory_order_relaxed);
    if (abs(newPosition - last) > minMovementThreshold)
    {
        setPosition(newPosition);
        lastPosition.store(newPosition, std::memory_order_relaxed);
    }
}