#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cstdint>

namespace darkgrain::dsp
{
/**
    Fixed-size stereo circular buffer holding the last few seconds of input.

    Reads use linear interpolation and always wrap safely. When frozen, the
    write head stops advancing so the held material is recycled by the grain and
    particle engines.
*/
class GrainBuffer
{
public:
    static constexpr double historySeconds = 4.0;

    void prepare (double newSampleRate);
    void reset();

    void setFrozen (bool shouldFreeze) noexcept;
    void write (const juce::AudioBuffer<float>& input, int numSamples) noexcept;

    float read (int channel, double position) const noexcept;

    int getLength() const noexcept { return buffer.getNumSamples(); }
    int getAvailableHistory() const noexcept { return availableHistory; }
    int getWritePosition() const noexcept { return writeIndex; }

private:
    juce::AudioBuffer<float> buffer;
    double sampleRate = 48000.0;
    int writeIndex = 0;
    int availableHistory = 0;
    juce::SmoothedValue<float> writeGain;
};

struct GrainParameters
{
    float sizeMs = 90.0f;
    float density = 12.0f;
    float spread = 0.35f;
    float pitchSemitones = 0.0f;
    float pitchScatterSemitones = 0.0f;
    float reverseAmount = 0.2f;
    float stereoSpread = 0.45f;
    bool freeze = false;
    std::uint16_t scaleMask = 0x0fff;
};

/**
    Overlap-add granular engine.

    Grains are windowed with a Hann envelope, read from GrainBuffer with
    fractional positions, and can be pitched, scattered, reversed and panned.
    All memory is allocated in prepare(); process() never allocates or locks.
*/
class GrainEngine
{
public:
    void prepare (double newSampleRate, double historySeconds = GrainBuffer::historySeconds);
    void reset();

    void setParameters (const GrainParameters& newParameters) noexcept;

    void process (const juce::AudioBuffer<float>& input,
                  juce::AudioBuffer<float>& output,
                  int numSamples) noexcept;

    const GrainBuffer& getBuffer() const noexcept { return grainBuffer; }

    static constexpr int maxGrains = 72;

private:
    struct Grain
    {
        double position = 0.0;
        double increment = 1.0;
        double offsetLeft = 0.0;
        double offsetRight = 0.0;
        int lengthSamples = 0;
        int age = 0;
        float panLeft = 0.0f;
        float panRight = 0.0f;
        float amplitude = 1.0f;
    };

    void spawnGrain() noexcept;
    float windowAt (int age, int lengthSamples) const noexcept;

    static constexpr int windowTableSize = 4096;

    GrainBuffer grainBuffer;
    std::array<Grain, maxGrains> grains;
    std::array<float, windowTableSize + 1> windowTable {};

    juce::Random random { 0x6d2b79f5u };
    juce::SmoothedValue<float> normalisation;

    double sampleRate = 48000.0;
    double grainPhase = 0.0;
    int numActive = 0;

    GrainParameters parameters;

    JUCE_LEAK_DETECTOR (GrainEngine)
};
} // namespace darkgrain::dsp
