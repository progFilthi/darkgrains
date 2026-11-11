#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>

namespace darkgrain::dsp
{
/**
    Subtle, musical saturation.

    The drive is deliberately modest and the wet blend follows the control so
    the stage can be bypassed completely at 0%. The output is level-compensated
    to avoid an obvious volume jump as the control is raised.
*/
class Saturation
{
public:
    void prepare (double newSampleRate) noexcept
    {
        amount.reset (newSampleRate > 0.0 ? newSampleRate : 48000.0, 0.03);
        amount.setCurrentAndTargetValue (0.0f);
    }

    void reset() noexcept {}

    void setAmount (float amount01) noexcept
    {
        amount.setTargetValue (juce::jlimit (0.0f, 1.0f, amount01));
    }

    float process (float input) noexcept
    {
        const auto blend = amount.getNextValue();
        const auto drive = 1.0f + 9.0f * blend;
        const auto shaped = std::tanh (input * drive) / std::tanh (drive);
        const auto compensated = shaped * (1.0f - 0.22f * blend);

        return input + (compensated - input) * blend;
    }

private:
    juce::SmoothedValue<float> amount;
};
} // namespace darkgrain::dsp
