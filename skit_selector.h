#ifndef SKIT_SELECTOR_H
#define SKIT_SELECTOR_H

#include <vector>
#include <string>
#include "parsed_skit.h"

class SkitSelector
{
public:
    SkitSelector(const std::vector<ParsedSkit> &skits);
    ParsedSkit selectNextSkit();
    void updateSkitPlayCount(const String &skitName);

private:
    struct SkitStats
    {
        ParsedSkit skit;
        int playCount;
        unsigned long lastPlayedTime;
    };
    std::vector<SkitStats> m_skitStats;
    String m_lastPlayedSkitName;

    double calculateSkitWeight(const SkitStats &stats, unsigned long currentTime);
    void sortSkitsByWeight();
};

#endif // SKIT_SELECTOR_H