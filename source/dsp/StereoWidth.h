#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace darkgrain::dsp
{
/** Mid/side stereo width control with click-free width changes. */
class StereoWidth
{
public:
    void prepare (double newSampleRate) noexcept
    {
        width.reset (newSampleRate > 0.0 ? newSampleRate : 48000.0, 0.03);
        width.setCurrentAndTargetValue (1.0f);
    }

    void reset() noexcept {}

    void setWidth (float newWidth) noexcept
    {
        width.setTargetValue (juce::jlimit (0.0f, 2.0f, newWidth));
    }

    void process (float& left, float& right) noexcept
    {
        const auto currentWidth = width.getNextValue();
        const auto mid = (left + right) * 0.5f;
        const auto side = (left - right) * 0.5f * currentWidth;

        left = mid + side;
        right = mid - side;
    }

private:
    juce::SmoothedValue<float> width;
};
} // namespace darkgrain::dsp
