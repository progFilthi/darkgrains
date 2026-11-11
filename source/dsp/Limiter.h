#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>

namespace darkgrain::dsp
{
/**
    Memoryless safety clipper.

    The curve is transparent below the threshold and asymptotically approaches
    full scale, so extreme feedback, freeze and automation settings can never
    push the output past 0 dBFS.
*/
class Limiter
{
public:
    void prepare (double) noexcept {}
    void reset() noexcept {}

    static float softClip (float input) noexcept
    {
        // Transparent right up to full scale; only material that would clip is
        // shaped. Keeping the threshold this high means a clean, gain-applied
        // signal passes through bit-accurately.
        constexpr float threshold = 0.999f;
        constexpr float headroom = 1.0f - threshold;

        if (input > threshold)
            return threshold + headroom * std::tanh ((input - threshold) / headroom);

        if (input < -threshold)
            return -threshold - headroom * std::tanh ((-input - threshold) / headroom);

        return input;
    }

    float process (float input) const noexcept { return softClip (input); }
};
} // namespace darkgrain::dsp
