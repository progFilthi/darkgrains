#include "dsp/Scale.h"

#include <juce_core/juce_core.h>

#include <array>

class DarkGrainScaleTests final : public juce::UnitTest
{
public:
    DarkGrainScaleTests()
        : juce::UnitTest ("Scale quantisation", "DarkGrain")
    {
    }

    void runTest() override
    {
        using namespace darkgrain::dsp::scales;

        beginTest ("Chromatic mode leaves offsets untouched");

        expectEquals (quantizeSemitones (0.0f, chromatic), 0);
        expectEquals (quantizeSemitones (1.0f, chromatic), 1);
        expectEquals (quantizeSemitones (-7.0f, chromatic), -7);
        expectEquals (quantizeSemitones (12.4f, chromatic), 12);

        beginTest ("Minor scale snaps to the nearest diatonic interval");

        const auto minor = maskForIndex (1);

        expectEquals (quantizeSemitones (0.0f, minor), 0);
        expectEquals (quantizeSemitones (1.0f, minor), 0);
        expectEquals (quantizeSemitones (2.0f, minor), 2);
        expectEquals (quantizeSemitones (4.0f, minor), 3);
        expectEquals (quantizeSemitones (6.0f, minor), 5);
        expectEquals (quantizeSemitones (-1.0f, minor), -2);
        expectEquals (quantizeSemitones (-7.0f, minor), -7);
        expectEquals (quantizeSemitones (12.0f, minor), 12);

        beginTest ("Major scale snaps to the nearest diatonic interval");

        const auto major = maskForIndex (2);

        expectEquals (quantizeSemitones (0.0f, major), 0);
        expectEquals (quantizeSemitones (1.0f, major), 0);
        expectEquals (quantizeSemitones (3.0f, major), 2);
        expectEquals (quantizeSemitones (6.0f, major), 5);
        expectEquals (quantizeSemitones (10.0f, major), 9);

        beginTest ("Pentatonic minor keeps only its five degrees");

        const auto pentatonic = maskForIndex (8);

        expectEquals (quantizeSemitones (0.0f, pentatonic), 0);
        expectEquals (quantizeSemitones (2.0f, pentatonic), 3);
        expectEquals (quantizeSemitones (4.0f, pentatonic), 3);
        expectEquals (quantizeSemitones (6.0f, pentatonic), 5);
        expectEquals (quantizeSemitones (8.0f, pentatonic), 7);

        beginTest ("Every scale includes its root and octave");

        for (int index = 0; index < 10; ++index)
        {
            const auto mask = maskForIndex (index);
            expectEquals (quantizeSemitones (0.0f, mask), 0);
            expectEquals (quantizeSemitones (12.0f, mask), 12);
            expectEquals (quantizeSemitones (-12.0f, mask), -12);
        }
    }
};

static DarkGrainScaleTests darkGrainScaleTests;
