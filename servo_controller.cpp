#include "servo_controller.h"
#include <algorithm>
#include <cmath>

// ServoController class constructor
// Initializes member variables with default values
ServoController::ServoController()
    : servoPin(-1), currentPosition(0), minDegrees(0), maxDegrees(0),
      smoothedPosition(0), lastPosition(0), maxObservedRMS(0) {}

// Initialize the servo controller with specified parameters
void ServoController::initialize(int pin, int minDeg, int maxDeg)
{
    // Set member variables with provided values
    servoPin = pin;
    minDegrees = minDeg;
    maxDegrees = maxDeg;
    currentPosition = minDeg;

    // Log initialization details
    Serial.printf("Initializing servo on pin %d (min: %d, max: %d)\n", servoPin, minDegrees, maxDegrees);

    // Attach servo to the specified pin and set initial position
    servo.attach(servoPin);
    setPosition(currentPosition);

    // Perform an initialization animation to demonstrate servo range
    Serial.printf("Servo animation init: %d (min) degrees\n", minDegrees);
    setPosition(minDegrees);
    Serial.printf("Servo animation init: %d (max) degrees\n", maxDegrees);
    delay(500);
    setPosition(maxDegrees);
    Serial.println("Servo animation init complete; resetting to 0 degrees");
    delay(500);
    setPosition(minDegrees);
}

// Set the servo position within the allowed range
void ServoController::setPosition(int degrees)
{
    // Ensure the position is within the allowed range
    int constrainedDegrees = constrain(degrees, minDegrees, maxDegrees);

    // Write the position to the servo
    servo.write(servoPin, constrainedDegrees);

    // Update the current position (thread-safe)
    currentPosition.store(constrainedDegrees, std::memory_order_relaxed);
}

// Map RMS (Root Mean Square) audio level to servo position
int ServoController::mapRMSToPosition(double rms, double silenceThreshold)
{
    // If RMS is below the silence threshold, return minimum position
    if (rms < silenceThreshold)
    {
        return minDegrees;
    }

    // Update maximum observed RMS if necessary
    if (rms > maxObservedRMS)
    {
        maxObservedRMS = rms;
    }

    // Normalize RMS value
    double normalizedRMS = std::min(rms / maxObservedRMS, 1.0);

    // Apply non-linear mapping to create more natural movement
    double mappedValue = pow(normalizedRMS, 0.2); // 0.2 is the MOVE_EXPONENT

    // Define minimum jaw opening to prevent complete closure
    int minJawOpening = minDegrees + 5; // 5 degrees minimum opening

    // Map the value to servo position range
    return map(mappedValue * 1000, 0, 1000, minJawOpening, maxDegrees);
}

// Update servo position with smoothing and minimum movement threshold
void ServoController::updatePosition(int targetPosition, double alpha, int minMovementThreshold)
{
    // Load current smoothed position
    double current = smoothedPosition.load(std::memory_order_relaxed);

    // Apply exponential smoothing
    double updated = alpha * targetPosition + (1 - alpha) * current;

    // Store updated smoothed position
    smoothedPosition.store(updated, std::memory_order_relaxed);

    // Round and constrain the new position
    int newPosition = round(updated);
    newPosition = constrain(newPosition, minDegrees, maxDegrees);

    // Load last position
    int last = lastPosition.load(std::memory_order_relaxed);

    // Only update position if change exceeds minimum movement threshold
    if (abs(newPosition - last) > minMovementThreshold)
    {
        setPosition(newPosition);
        lastPosition.store(newPosition, std::memory_order_relaxed);
    }
}