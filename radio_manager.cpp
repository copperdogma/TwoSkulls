#include "radio_manager.h"
#include <Arduino.h>

RadioManager::RadioManager()
    : currentOwner(""), accessExpirationTime(0) {}

std::string RadioManager::checkAndUpdateOwner()
{
    if (millis() >= accessExpirationTime)
    {
        currentOwner = "";
    }
    return currentOwner;
}

bool RadioManager::requestAccess(const std::string &requester, unsigned long timeoutMs)
{
    std::lock_guard<std::mutex> lock(mtx);

    std::string owner = checkAndUpdateOwner();

    // If no one owns the radio or the requester is already the owner, grant access
    if (owner.empty() || owner == requester)
    {
        currentOwner = requester;
        accessExpirationTime = millis() + timeoutMs;
        //Serial.printf("RadioManager: Access granted to '%s' until %lu\n", requester.c_str(), accessExpirationTime);
        return true;
    }

    //Serial.printf("RadioManager: Access denied for '%s'. Current owner: '%s', expiration time: %lu\n", requester.c_str(), owner.c_str(), accessExpirationTime);
    return false;
}

std::string RadioManager::getCurrentOwner()
{
    std::lock_guard<std::mutex> lock(mtx);
    return checkAndUpdateOwner();
}