#ifndef RADIO_MANAGER_H
#define RADIO_MANAGER_H

#include <Arduino.h>
#include <queue>
#include <string>
#include <mutex>
#include <condition_variable>
#include <chrono>

/**
 * @brief Manages exclusive access to the radio resource.
 *
 * This class handles access requests from multiple modules, ensuring that only one
 * requester has access to the radio at any given time. It incorporates thread safety,
 * timeouts, logging, and exposes the current state of the radio.
 */
class RadioManager
{
public:
    RadioManager();

    /**
     * @brief Request access to the radio.
     *
     * @param requester A string identifier for the requester.
     * @param timeoutMs Maximum time in milliseconds to wait for access.
     * @return true if access was granted, false otherwise.
     */
    bool requestAccess(const std::string &requester, unsigned long timeoutMs = 1000);

    /**
     * @brief Release access to the radio.
     *
     * @param requester A string identifier for the requester.
     */
    void releaseAccess(const std::string &requester);

    /**
     * @brief Get the current owner of the radio.
     *
     * @return std::string The requester currently holding the radio. Empty if none.
     */
    std::string getCurrentOwner();

private:
    static const unsigned long GRACE_PERIOD_MS = 200;  // Constant grace period

    std::mutex mtx;
    std::condition_variable cv;
    std::string currentOwner;
    std::queue<std::string> requestQueue;
    bool isReleasing;

    /**
     * @brief Logs messages with a consistent format.
     *
     * @param message The message to log.
     */
    void log(const std::string &message);

    /**
     * @brief Handles the transition after releasing the radio.
     */
    void handleNext();
};

#endif // RADIO_MANAGER_H