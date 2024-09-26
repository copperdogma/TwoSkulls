#ifndef SERVO_CONTROLLER_H
#define SERVO_CONTROLLER_H

#include <Servo.h>
#include <atomic>

class ServoController
{
private:
    Servo servo;                          // Servo object for controlling the physical servo
    int servoPin;                         // Pin number to which the servo is attached
    std::atomic<int> currentPosition;     // Current position of the servo (thread-safe)
    int minDegrees;                       // Minimum allowed position in degrees
    int maxDegrees;                       // Maximum allowed position in degrees
    std::atomic<double> smoothedPosition; // Smoothed position for gradual movements (thread-safe)
    std::atomic<int> lastPosition;        // Last set position (thread-safe)
    std::atomic<double> maxObservedRMS;   // Maximum observed RMS value for audio mapping (thread-safe)

public:
    // Constructor: Initializes member variables with default values
    ServoController();

    // Initializes the servo controller with specified parameters
    void initialize(int pin, int minDeg, int maxDeg);

    // Sets the servo position within the allowed range
    void setPosition(int degrees);

    // Returns the current position of the servo (thread-safe)
    int getCurrentPosition() const { return currentPosition.load(std::memory_order_relaxed); }

    // Maps RMS (Root Mean Square) audio level to servo position
    int mapRMSToPosition(double rms, double silenceThreshold);

    // Updates servo position with smoothing and minimum movement threshold
    void updatePosition(int targetPosition, double alpha, int minMovementThreshold);
};

#endif // SERVO_CONTROLLER_H