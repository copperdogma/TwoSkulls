#include "radio_manager.h"

RadioManager::RadioManager()
    : currentOwner(""), isReleasing(false) {}

bool RadioManager::requestAccess(const std::string &requester, unsigned long timeoutMs)
{
    std::unique_lock<std::mutex> lock(mtx);

    // If the requester is already the current owner, grant access immediately
    if (currentOwner == requester)
    {
        log("Requester '" + requester + "' already owns the radio.");
        return true;
    }

    // If no one owns the radio, grant access
    if (currentOwner.empty())
    {
        currentOwner = requester;
        log("Access granted to '" + requester + "'.");
        return true;
    }

    // Add to the queue
    requestQueue.push(requester);
    log("Requester '" + requester + "' added to the queue.");

    // Wait for access or timeout
    auto start = std::chrono::steady_clock::now();
    while (currentOwner != requester)
    {
        if (cv.wait_until(lock, start + std::chrono::milliseconds(timeoutMs)) == std::cv_status::timeout)
        {
            // Timeout occurred
            // Remove requester from the queue
            std::queue<std::string> tempQueue;
            while (!requestQueue.empty())
            {
                if (requestQueue.front() != requester)
                {
                    tempQueue.push(requestQueue.front());
                }
                requestQueue.pop();
            }
            requestQueue = tempQueue;
            log("Timeout: Requester '" + requester + "' could not acquire the radio.");
            return false;
        }
    }

    // Access granted
    log("Access granted to '" + requester + "'.");
    return true;
}

void RadioManager::releaseAccess(const std::string &requester)
{
    std::unique_lock<std::mutex> lock(mtx);

    if (currentOwner != requester)
    {
        log("Requester '" + requester + "' attempted to release the radio but does not own it.");
        return;
    }

    log("Requester '" + requester + "' is releasing the radio.");
    isReleasing = true;
    currentOwner = "";
    lock.unlock();

    // Start grace period
    delay(GRACE_PERIOD_MS);

    lock.lock();
    isReleasing = false;
    handleNext();
}

std::string RadioManager::getCurrentOwner()
{
    std::lock_guard<std::mutex> lock(mtx);
    return currentOwner;
}

void RadioManager::log(const std::string &message)
{
    Serial.println(("RadioManager: " + message).c_str());
}

void RadioManager::handleNext()
{
    if (!requestQueue.empty())
    {
        currentOwner = requestQueue.front();
        requestQueue.pop();
        log("Access granted to next requester '" + currentOwner + "'.");
        cv.notify_all();
    }
    else
    {
        log("Radio is now free.");
        cv.notify_all();
    }
}