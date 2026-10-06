#pragma once

#include <JuceHeader.h>
#include <array>

namespace OpalFrequencyData
{
    struct Entry
    {
        float hz;
        const char* label;
    };

    inline constexpr std::array<Entry, 18> entries {{
        { 111.0f, "Intention"  },
        { 174.0f, "Foundation" },
        { 222.0f, "Balance"    },
        { 285.0f, "Renewal"    },
        { 333.0f, "Expression" },
        { 396.0f, "Release"    },
        { 417.0f, "Transition" },
        { 432.0f, "Natural"    },
        { 444.0f, "Stability"  },
        { 528.0f, "Center"     },
        { 555.0f, "Change"     },
        { 639.0f, "Connection" },
        { 741.0f, "Clarity"    },
        { 777.0f, "Insight"    },
        { 852.0f, "Intuition"  },
        { 888.0f, "Abundance"  },
        { 963.0f, "Unity"      },
        { 999.0f, "Completion" }
    }};

    inline juce::StringArray makeChoiceStrings()
    {
        juce::StringArray result;

        for (const auto& entry : entries)
            result.add (juce::String (static_cast<int> (entry.hz)) + " Hz");

        return result;
    }

    inline int clampIndex (int index)
    {
        return juce::jlimit (0, static_cast<int> (entries.size()) - 1, index);
    }

    inline float frequencyForIndex (int index)
    {
        return entries[static_cast<size_t> (clampIndex (index))].hz;
    }

    inline juce::String labelForIndex (int index)
    {
        return entries[static_cast<size_t> (clampIndex (index))].label;
    }
}
