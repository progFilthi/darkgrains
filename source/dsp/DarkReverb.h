#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>

namespace darkgrain::dsp
{
/**
    Dark FDN (feedback delay network) reverb.

    The reverb uses eight delay lines with mutually prime lengths, a normalised
    Hadamard feedback matrix, per-line damping, all-pass input diffusion and slow
    LFO modulation. It has a stereo pre-delay and produces a decorrelated stereo
    wet signal.

    All memory is allocated in prepare(). process() performs no allocation, no
    locking, no file I/O and no logging, so it is safe to call from the audio
    callback.
*/
class DarkReverb
{
public:
    struct Parameters
    {
        /** Reverb decay time (RT60) in seconds. Range: 0.1 .. 10.0 */
        float sizeSeconds = 2.5f;
        /** High-frequency damping. Range: 0.0 (bright) .. 1.0 (dark). */
        float damping = 0.55f;
        /** Input diffusion / smearing. Range: 0.0 .. 1.0. */
        float diffusion = 0.6f;
        /** Slow modulation rate in Hz. Range: 0.01 .. 2.0. */
        float modulationRateHz = 0.15f;
        /** Modulation depth. Range: 0.0 .. 1.0. */
        float modulationDepth = 0.25f;
        /** Stereo pre-delay in milliseconds. Range: 0.0 .. 500.0. */
        float preDelayMs = 35.0f;
        /** Stereo width of the wet signal. Range: 0.0 (mono) .. 2.0 (wide). */
        float width = 1.2f;
        /** Near-infinite sustain when true. */
        bool freeze = false;
    };

    void prepare (double newSampleRate, int maximumBlockSize);
    void reset();

    /** Sets the smoothing targets. Call once per block, then process(). */
    void setParameters (const Parameters& newParameters) noexcept;

    void process (const juce::AudioBuffer<float>& input,
                  juce::AudioBuffer<float>& output,
                  int numSamples) noexcept;

    double getMaximumTailSeconds() const noexcept;

    static constexpr int numLines = 8;
    static constexpr float maximumPreDelaySeconds = 0.5f;
    static constexpr float maximumModulationSeconds = 0.004f;

private:
    struct Allpass
    {
        juce::AudioBuffer<float> buffer;
        int writeIndex = 0;

        float process (float input, float coefficient) noexcept;
        void reset() noexcept;
    };

    struct Line
    {
        juce::AudioBuffer<float> buffer;
        int writeIndex = 0;
        int baseDelaySamples = 0;
        float dampState = 0.0f;
        float lowCutState = 0.0f;
        float modPhase = 0.0f;
        juce::SmoothedValue<float> feedbackGain;

        void reset() noexcept;
    };

    float readLine (const Line& line, float delaySamples) const noexcept;
    float readPreDelay (int channel, float delaySamples) const noexcept;
    void updateDerivedParameters (const Parameters& newParameters) noexcept;
    void snapSmoothedValues() noexcept;

    double sampleRate = 48000.0;

    std::array<Line, numLines> lines;
    std::array<std::array<Allpass, 4>, 2> diffusers;
    juce::AudioBuffer<float> preDelayBuffer;
    int preDelayWriteIndex = 0;

    juce::SmoothedValue<float> preDelaySamples;
    juce::SmoothedValue<float> diffusionCoefficient;
    juce::SmoothedValue<float> dampingCoefficient;
    juce::SmoothedValue<float> modulationDepthSamples;
    juce::SmoothedValue<float> freezeAmount;
    juce::SmoothedValue<float> width;

    float modulationIncrement = 0.0f;
    float lowCutCoefficient = 0.0f;

    JUCE_LEAK_DETECTOR (DarkReverb)
};
} // namespace darkgrain::dsp
