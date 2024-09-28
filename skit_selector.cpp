#include "skit_selector.h"
#include <algorithm>
#include <cmath>
#include <Arduino.h>

// SkitSelector class manages the selection and playback of skits
// It uses a weighted random selection algorithm to ensure variety and fairness in skit playback
class SkitSelector
{
public:
    // Constructor: Initializes the SkitSelector with a list of parsed skits
    SkitSelector(const std::vector<ParsedSkit> &skits)
    {
        for (const auto &skit : skits)
        {
            m_skitStats.push_back({skit, 0, 0});
        }
    }

    // Selects the next skit to be played based on weighted random selection
    ParsedSkit selectNextSkit()
    {
        unsigned long currentTime = millis();
        sortSkitsByWeight();

        // Select a skit randomly from the top 3 weighted skits (or fewer if less than 3 available)
        int selectionPool = std::min(3, static_cast<int>(m_skitStats.size()));
        int selectedIndex = random(selectionPool);

        // Debug output: List all skits in the selection pool and their weights
        for (int i = 0; i < selectionPool; i++)
        {
            Serial.printf("SkitSelector::selectNextSkit: Skit %d: %s, weight: %f\n", i, m_skitStats[i].skit.audioFile.c_str(), calculateSkitWeight(m_skitStats[i], currentTime));
        }

        // Update the selected skit's play count and last played time
        auto &selectedSkit = m_skitStats[selectedIndex];
        selectedSkit.playCount++;
        selectedSkit.lastPlayedTime = currentTime;
        m_lastPlayedSkitName = selectedSkit.skit.audioFile;

        return selectedSkit.skit;
    }

    // Updates the play count and last played time for a specific skit
    void updateSkitPlayCount(const String &skitName)
    {
        auto it = std::find_if(m_skitStats.begin(), m_skitStats.end(),
                               [&skitName](const SkitStats &stats)
                               { return stats.skit.audioFile == skitName; });
        if (it != m_skitStats.end())
        {
            it->playCount++;
            it->lastPlayedTime = millis();
        }
    }

private:
    // Struct to hold statistics for each skit
    struct SkitStats
    {
        ParsedSkit skit;
        int playCount;
        unsigned long lastPlayedTime;
    };

    // Calculates the weight of a skit based on its play count and last played time
    // This weight is used to prioritize skits that haven't been played recently or frequently
    double calculateSkitWeight(const SkitStats &stats, unsigned long currentTime)
    {
        // Use logarithmic time factor to gradually increase priority over time
        double timeFactor = log(currentTime - stats.lastPlayedTime + 1);
        // Inverse play count factor to prioritize less frequently played skits
        double playCountFactor = 1.0 / (stats.playCount + 1);
        return timeFactor * playCountFactor;
    }

    // Sorts the skits by their calculated weights in descending order
    void sortSkitsByWeight()
    {
        unsigned long currentTime = millis();
        std::sort(m_skitStats.begin(), m_skitStats.end(),
                  [this, currentTime](const SkitStats &a, const SkitStats &b)
                  {
                      return calculateSkitWeight(a, currentTime) > calculateSkitWeight(b, currentTime);
                  });
    }

    std::vector<SkitStats> m_skitStats;
    String m_lastPlayedSkitName;
};