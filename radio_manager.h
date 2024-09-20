#ifndef RADIO_MANAGER_H
#define RADIO_MANAGER_H

#include <Arduino.h>
#include <string>
#include <mutex>

/**
 * @brief Manages exclusive access to the radio resource.
 *
 * This class handles access requests from multiple modules, ensuring that only one
 * requester has access to the radio at any given time. It incorporates thread safety,
 * timeouts, and exposes the current state of the radio.
 */
class RadioManager
{
public:
    RadioManager();

    /**
     * @brief Request access to the radio.
     *
     * @param requester A string identifier for the requester.
     * @param timeoutMs Time in milliseconds for which the access is granted.
     * @return true if access was granted, false otherwise.
     */
    bool requestAccess(const std::string &requester, unsigned long timeoutMs);

    /**
     * @brief Get the current owner of the radio.
     *
     * @return std::string The requester currently holding the radio. Empty if none.
     */
    std::string getCurrentOwner();

private:
    std::mutex mtx;
    std::string currentOwner;
    unsigned long accessExpirationTime;

    /**
     * @brief Checks if the current access has expired and updates the owner accordingly.
     *
     * @return std::string The current owner after checking and updating.
     */
    std::string checkAndUpdateOwner();
};

#endif // RADIO_MANAGER_H