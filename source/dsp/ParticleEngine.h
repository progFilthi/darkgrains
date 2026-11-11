#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "dsp/Filter.h"
#include "dsp/GrainEngine.h"

#include <array>
#include <cstdint>

namespace darkgrain::dsp
{
struct ParticleParameters
{
    float density = 0.35f;
    float sizeMs = 70.0f;
    float brightness = 0.4f;
    float pitchSemitones = -7.0f;
    float panWidth = 0.55f;
    float randomness = 0.5f;
    float feedback = 0.2f;
    float mix = 0.3f;
};

/**
    Short randomized textural events read from the granular history.

    Each particle is a band-passed, pitch-shifted grain with a Hann envelope,
    randomized pan, and an optional breath of filtered noise. A short internal
    feedback delay lets particles smear into each other without being able to
    run away.
*/
class ParticleEngine
{
public:
    void prepare (double newSampleRate);
    void reset();

    void setSource (const GrainBuffer* newSource) noexcept { source = newSource; }
    void setParameters (const ParticleParameters& newParameters) noexcept;

    void process (juce::AudioBuffer<float>& output, int numSamples) noexcept;

    static constexpr int maxParticles = 48;

private:
    struct Particle
    {
        double position = 0.0;
        double increment = 1.0;
        int lengthSamples = 0;
        int age = 0;
        float panLeft = 0.0f;
        float panRight = 0.0f;
        float amplitude = 1.0f;
        float noiseAmount = 0.0f;
        StateVariableBandPass filterLeft;
        StateVariableBandPass filterRight;
    };

    void spawnParticle() noexcept;
    float windowAt (int age, int lengthSamples) const noexcept;
    float nextNoise() noexcept;

    static constexpr int windowTableSize = 1024;

    const GrainBuffer* source = nullptr;

    std::array<Particle, maxParticles> particles;
    std::array<float, windowTableSize + 1> windowTable {};
    juce::AudioBuffer<float> feedbackBuffer;

    juce::Random random { 0x2f9a3c11u };
    juce::SmoothedValue<float> mixGain;
    juce::SmoothedValue<float> feedbackGain;

    OnePoleLowPass feedbackLowPassLeft;
    OnePoleLowPass feedbackLowPassRight;

    double sampleRate = 48000.0;
    double eventPhase = 0.0;
    int numActive = 0;
    int feedbackIndex = 0;
    std::uint32_t noiseState = 0x9e3779b9u;

    ParticleParameters parameters;

    JUCE_LEAK_DETECTOR (ParticleEngine)
};
} // namespace darkgrain::dsp
