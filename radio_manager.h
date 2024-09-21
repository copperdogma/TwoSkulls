#ifndef RADIO_MANAGER_H
#define RADIO_MANAGER_H

#include <Arduino.h>
#include <string>
#include <mutex>

/**
 * @brief Manages exclusive access to the radio resource.
 * 
 * Theory of Operation:
 * - The RadioManager uses a time-based access system to manage the radio resource.
 * - Requesters are granted access for a specified duration.
 * - Access automatically expires after the granted duration.
 * - No explicit release of access is required.
 * 
 * Expected Usage:
 * 1. Components request access using requestAccess() with their identifier and desired access duration.
 * 2. If granted, the component can use the radio for the specified duration.
 * 3. Components should re-request access periodically if they need continued access.
 * 4. Access is automatically released after the specified duration, allowing other components to gain access.
 * 
 * This approach ensures efficient use of the radio resource and prevents deadlocks or resource hogging.
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