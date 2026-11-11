#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>

namespace darkgrain::dsp
{
/** Simple one-pole low-pass used for damping and tone shaping. */
class OnePoleLowPass
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        setCutoff (cutoffHz);
        reset();
    }

    void reset() noexcept { state = 0.0f; }

    void setCutoff (float hz) noexcept
    {
        cutoffHz = hz;
        const auto clamped = juce::jlimit (10.0f, static_cast<float> (sampleRate) * 0.49f, hz);
        coefficient = 1.0f - std::exp (-juce::MathConstants<float>::twoPi
                                       * clamped / static_cast<float> (sampleRate));
    }

    float process (float input) noexcept
    {
        state += coefficient * (input - state);
        return state;
    }

private:
    double sampleRate = 48000.0;
    float cutoffHz = 20000.0f;
    float coefficient = 1.0f;
    float state = 0.0f;
};

/** One-pole high-pass built from the complementary low-pass state. */
class OnePoleHighPass
{
public:
    void prepare (double newSampleRate) noexcept
    {
        lowPass.prepare (newSampleRate);
    }

    void reset() noexcept { lowPass.reset(); }

    void setCutoff (float hz) noexcept { lowPass.setCutoff (hz); }

    float process (float input) noexcept
    {
        return input - lowPass.process (input);
    }

private:
    OnePoleLowPass lowPass;
};

/**
    Dark tilt filter: a low shelf and high shelf moving in opposite directions.
    A value of 0 leaves the signal untouched, 1 rolls the highs off and adds
    low-end weight.
*/
class TiltFilter
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        lowSplit.prepare (sampleRate);
        lowSplit.setCutoff (400.0f);

        lowGain.reset (sampleRate, 0.05);
        highGain.reset (sampleRate, 0.05);
        lowGain.setCurrentAndTargetValue (1.0f);
        highGain.setCurrentAndTargetValue (1.0f);
    }

    void reset() noexcept
    {
        lowSplit.reset();
    }

    void setDarkness (float amount01) noexcept
    {
        const auto amount = juce::jlimit (0.0f, 1.0f, amount01);
        lowGain.setTargetValue (1.0f + 0.55f * amount);
        highGain.setTargetValue (1.0f - 0.82f * amount);
    }

    float process (float input) noexcept
    {
        const auto low = lowSplit.process (input);
        return low * lowGain.getNextValue() + (input - low) * highGain.getNextValue();
    }

private:
    double sampleRate = 48000.0;
    OnePoleLowPass lowSplit;
    juce::SmoothedValue<float> lowGain;
    juce::SmoothedValue<float> highGain;
};

/** TPT state-variable band-pass with a normalised peak gain. */
class StateVariableBandPass
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        updateCoefficients();
        reset();
    }

    void reset() noexcept
    {
        ic1eq = 0.0f;
        ic2eq = 0.0f;
    }

    void setCutoff (float hz) noexcept
    {
        cutoffHz = juce::jlimit (30.0f, static_cast<float> (sampleRate) * 0.45f, hz);
        updateCoefficients();
    }

    void setResonance (float q) noexcept
    {
        resonance = juce::jmax (0.2f, q);
        updateCoefficients();
    }

    float process (float input) noexcept
    {
        const auto v3 = input - ic2eq;
        const auto v1 = a1 * ic1eq + a2 * v3;
        const auto v2 = ic2eq + a2 * ic1eq + a3 * v3;

        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;

        return v1 * k;
    }

private:
    void updateCoefficients() noexcept
    {
        g = std::tan (juce::MathConstants<float>::pi * cutoffHz / static_cast<float> (sampleRate));
        k = 1.0f / resonance;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    double sampleRate = 48000.0;
    float cutoffHz = 2000.0f;
    float resonance = 0.9f;
    float g = 0.0f;
    float k = 1.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
    float a3 = 0.0f;
    float ic1eq = 0.0f;
    float ic2eq = 0.0f;
};
} // namespace darkgrain::dsp
