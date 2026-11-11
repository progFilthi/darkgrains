#include "dsp/ParticleEngine.h"

#include "dsp/Scale.h"

#include <cmath>

namespace darkgrain::dsp
{
void ParticleEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;

    for (int index = 0; index <= windowTableSize; ++index)
    {
        const auto phase = static_cast<float> (index) / static_cast<float> (windowTableSize);
        windowTable[static_cast<size_t> (index)] =
            0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * phase);
    }

    const auto feedbackSamples = juce::jmax (2, static_cast<int> (0.12 * sampleRate));
    feedbackBuffer.setSize (2, feedbackSamples, false, true, true);
    feedbackBuffer.clear();

    feedbackLowPassLeft.prepare (sampleRate);
    feedbackLowPassLeft.setCutoff (5000.0f);
    feedbackLowPassRight.prepare (sampleRate);
    feedbackLowPassRight.setCutoff (5000.0f);

    mixGain.reset (sampleRate, 0.05);
    mixGain.setCurrentAndTargetValue (0.0f);
    feedbackGain.reset (sampleRate, 0.05);
    feedbackGain.setCurrentAndTargetValue (0.0f);

    reset();
}

void ParticleEngine::reset()
{
    if (feedbackBuffer.getNumSamples() > 0)
        feedbackBuffer.clear();

    feedbackLowPassLeft.reset();
    feedbackLowPassRight.reset();

    feedbackIndex = 0;
    numActive = 0;
    eventPhase = 0.0;
}

void ParticleEngine::setParameters (const ParticleParameters& newParameters) noexcept
{
    parameters = newParameters;

    const auto density = juce::jlimit (0.0f, 1.0f, parameters.density);
    const auto sizeSeconds = juce::jlimit (0.005f, 0.6f, parameters.sizeMs * 0.001f);
    const auto eventRate = 0.5f + density * density * 24.0f;
    const auto overlap = juce::jmax (1.0f, eventRate * sizeSeconds);

    mixGain.setTargetValue (juce::jlimit (0.0f, 1.0f, parameters.mix) * 0.8f / std::sqrt (overlap));
    feedbackGain.setTargetValue (juce::jlimit (0.0f, 1.0f, parameters.feedback) * 0.45f);
}

float ParticleEngine::nextNoise() noexcept
{
    noiseState ^= noiseState << 13;
    noiseState ^= noiseState >> 17;
    noiseState ^= noiseState << 5;

    return static_cast<float> (noiseState & 0x00ffffffu) / 8388608.0f - 1.0f;
}

float ParticleEngine::windowAt (int age, int lengthSamples) const noexcept
{
    if (lengthSamples <= 0)
        return 0.0f;

    const auto phase = static_cast<float> (age) / static_cast<float> (lengthSamples);
    const auto index = juce::jlimit (0, windowTableSize,
                                     static_cast<int> (phase * static_cast<float> (windowTableSize)));

    return windowTable[static_cast<size_t> (index)];
}

void ParticleEngine::spawnParticle() noexcept
{
    if (source == nullptr || numActive >= maxParticles || source->getLength() <= 0)
        return;

    const auto available = source->getAvailableHistory();

    if (available <= 8)
        return;

    const auto randomness = juce::jlimit (0.0f, 1.0f, parameters.randomness);
    const auto sizeSeconds = juce::jlimit (0.005f, 0.6f, parameters.sizeMs * 0.001f);
    const auto lengthSamples = juce::jmax (16, static_cast<int> (
        std::lround (sizeSeconds * (0.65 + 0.7 * static_cast<double> (random.nextFloat()))
                     * sampleRate)));

    const auto pitchOffset = (random.nextFloat() * 2.0f - 1.0f) * randomness * 9.0f;
    const auto requestedPitch = parameters.pitchSemitones + pitchOffset;
    const auto pitch = static_cast<float> (
        scales::quantizeSemitones (requestedPitch, parameters.scaleMask));
    const auto rate = static_cast<double> (std::pow (2.0, static_cast<double> (pitch) / 12.0));

    Particle particle;
    particle.lengthSamples = lengthSamples;
    particle.increment = rate;
    particle.age = 0;

    auto start = static_cast<double> (source->getWritePosition())
               - static_cast<double> (random.nextFloat()) * static_cast<double> (available);

    if (random.nextFloat() < randomness * 0.55f)
    {
        start += static_cast<double> (lengthSamples) * rate;
        particle.increment = -rate;
    }

    const auto lengthAsDouble = static_cast<double> (source->getLength());

    while (start < 0.0)
        start += lengthAsDouble;

    while (start >= lengthAsDouble)
        start -= lengthAsDouble;

    particle.position = start;

    const auto panWidth = juce::jlimit (0.0f, 1.0f, parameters.panWidth);
    const auto pan = (random.nextFloat() * 2.0f - 1.0f) * panWidth;
    const auto angle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
    particle.panLeft = std::cos (angle);
    particle.panRight = std::sin (angle);

    const auto brightness = juce::jlimit (0.0f, 1.0f, parameters.brightness);
    const auto cutoff = 320.0f * std::pow (26.0f, brightness);

    particle.filterLeft.prepare (sampleRate);
    particle.filterLeft.setCutoff (cutoff);
    particle.filterLeft.setResonance (1.15f);

    particle.filterRight.prepare (sampleRate);
    particle.filterRight.setCutoff (cutoff * (0.92f + 0.16f * random.nextFloat()));
    particle.filterRight.setResonance (1.15f);

    particle.noiseAmount = 0.06f + 0.32f * randomness;
    particle.amplitude = 0.55f + 0.45f * random.nextFloat();

    particles[static_cast<size_t> (numActive)] = particle;
    ++numActive;
}

void ParticleEngine::process (juce::AudioBuffer<float>& output, int numSamples) noexcept
{
    juce::ScopedNoDenormals noDenormals;

    const auto outputChannels = juce::jmin (2, output.getNumChannels());
    const auto samplesToProcess = juce::jmin (numSamples, output.getNumSamples());

    if (outputChannels <= 0 || samplesToProcess <= 0)
        return;

    output.clear (0, samplesToProcess);

    if (source == nullptr || feedbackBuffer.getNumSamples() <= 0)
        return;

    const auto density = juce::jlimit (0.0f, 1.0f, parameters.density);
    const auto eventRate = 0.5f + density * density * 24.0f;
    const auto increment = static_cast<double> (eventRate) / sampleRate;
    const auto feedbackLength = feedbackBuffer.getNumSamples();

    for (int sample = 0; sample < samplesToProcess; ++sample)
    {
        eventPhase += increment;

        while (eventPhase >= 1.0)
        {
            eventPhase -= 1.0;
            spawnParticle();
        }

        const auto noiseLeft = nextNoise();
        const auto noiseRight = nextNoise();
        auto left = 0.0f;
        auto right = 0.0f;

        for (int index = 0; index < numActive;)
        {
            auto& particle = particles[static_cast<size_t> (index)];
            const auto envelope = windowAt (particle.age, particle.lengthSamples);

            const auto rawLeft = source->read (0, particle.position)
                               + noiseLeft * particle.noiseAmount;
            const auto rawRight = source->read (1, particle.position)
                                + noiseRight * particle.noiseAmount;

            left += particle.filterLeft.process (rawLeft)
                  * envelope * particle.panLeft * particle.amplitude;
            right += particle.filterRight.process (rawRight)
                   * envelope * particle.panRight * particle.amplitude;

            particle.position += particle.increment;
            ++particle.age;

            if (particle.age >= particle.lengthSamples)
            {
                particles[static_cast<size_t> (index)] =
                    particles[static_cast<size_t> (numActive - 1)];
                --numActive;
            }
            else
            {
                ++index;
            }
        }

        const auto gain = mixGain.getNextValue();
        const auto feedback = feedbackGain.getNextValue();

        const auto delayedLeft = feedbackBuffer.getSample (0, feedbackIndex);
        const auto delayedRight = feedbackBuffer.getSample (1, feedbackIndex);
        const auto mixLeft = (left + delayedLeft * feedback) * gain;
        const auto mixRight = (right + delayedRight * feedback) * gain;

        feedbackBuffer.setSample (0, feedbackIndex,
                                  std::tanh (feedbackLowPassLeft.process (mixLeft)));
        feedbackBuffer.setSample (1, feedbackIndex,
                                  std::tanh (feedbackLowPassRight.process (mixRight)));

        feedbackIndex = feedbackIndex + 1 >= feedbackLength ? 0 : feedbackIndex + 1;

        output.setSample (0, sample, mixLeft);

        if (outputChannels > 1)
            output.setSample (1, sample, mixRight);
    }
}
} // namespace darkgrain::dsp
