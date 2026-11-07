#include "PluginProcessor.h"
#include "dsp/DarkReverb.h"
#include "parameters/ParameterIDs.h"

#include <array>
#include <cmath>

namespace
{
void fillImpulse (juce::AudioBuffer<float>& buffer, float amplitude = 0.8f)
{
    buffer.clear();
    buffer.setSample (0, 0, amplitude);
    buffer.setSample (1, 0, amplitude);
}

float blockRms (const juce::AudioBuffer<float>& buffer, int numSamples)
{
    double sum = 0.0;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const auto value = static_cast<double> (buffer.getSample (channel, sample));
            sum += value * value;
        }

    return static_cast<float> (
        std::sqrt (sum / static_cast<double> (juce::jmax (1, buffer.getNumChannels() * numSamples))));
}

bool allFinite (const juce::AudioBuffer<float>& buffer, int numSamples)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < numSamples; ++sample)
            if (! std::isfinite (buffer.getSample (channel, sample)))
                return false;

    return true;
}

void setParameter (DarkGrainAudioProcessor& processor, const char* id, float value)
{
    if (auto* parameter = processor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

void fillStereoSine (juce::AudioBuffer<float>& buffer, double sampleRate)
{
    const auto increment = juce::MathConstants<double>::twoPi * 220.0 / sampleRate;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto value = 0.25f * static_cast<float> (std::sin (increment * sample));

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample (channel, sample, value);
    }
}
} // namespace

class DarkGrainReverbTests final : public juce::UnitTest
{
public:
    DarkGrainReverbTests()
        : juce::UnitTest ("Dark reverb", "DarkGrain")
    {
    }

    void runTest() override
    {
        using namespace darkgrain::dsp;

        beginTest ("Reverb tail continues after the input stops and decays");

        {
            DarkReverb reverb;
            reverb.prepare (48000.0, 512);

            DarkReverb::Parameters parameters;
            parameters.sizeSeconds = 2.0f;
            parameters.damping = 0.4f;
            parameters.diffusion = 0.6f;
            parameters.preDelayMs = 0.0f;
            parameters.width = 1.0f;
            reverb.setParameters (parameters);
            reverb.reset();

            constexpr int blockSize = 512;
            juce::AudioBuffer<float> input (2, blockSize);
            juce::AudioBuffer<float> output (2, blockSize);

            fillImpulse (input, 0.8f);
            reverb.process (input, output, blockSize);
            expect (allFinite (output, blockSize));

            float earlyTail = 0.0f;
            float lateTail = 0.0f;

            for (int block = 0; block < 200; ++block)
            {
                input.clear();
                reverb.process (input, output, blockSize);

                expect (allFinite (output, blockSize));

                const auto level = blockRms (output, blockSize);

                if (block >= 2 && block < 12)
                    earlyTail = juce::jmax (earlyTail, level);

                if (block >= 190)
                    lateTail = juce::jmax (lateTail, level);
            }

            expect (earlyTail > 0.0f, "Reverb tail is silent after the input stops");
            expect (earlyTail > lateTail * 4.0f, "Reverb tail does not decay");
        }

        beginTest ("Stereo output is decorrelated");

        {
            DarkReverb reverb;
            reverb.prepare (48000.0, 256);

            DarkReverb::Parameters parameters;
            parameters.sizeSeconds = 3.0f;
            parameters.preDelayMs = 0.0f;
            parameters.width = 1.0f;
            reverb.setParameters (parameters);
            reverb.reset();

            juce::AudioBuffer<float> input (2, 256);
            juce::AudioBuffer<float> output (2, 256);

            fillImpulse (input, 0.8f);
            reverb.process (input, output, 256);

            float maximumDifference = 0.0f;

            for (int block = 0; block < 40; ++block)
            {
                input.clear();
                reverb.process (input, output, 256);

                for (int sample = 0; sample < 256; ++sample)
                    maximumDifference = juce::jmax (
                        maximumDifference,
                        std::abs (output.getSample (0, sample) - output.getSample (1, sample)));
            }

            expect (maximumDifference > 1.0e-4f, "Reverb output is mono");
        }

        beginTest ("Freeze sustains the tail and releases cleanly");

        {
            DarkReverb reverb;
            reverb.prepare (48000.0, 512);

            DarkReverb::Parameters parameters;
            parameters.sizeSeconds = 3.0f;
            parameters.damping = 0.0f;
            parameters.diffusion = 0.5f;
            parameters.preDelayMs = 0.0f;
            parameters.width = 1.0f;
            reverb.setParameters (parameters);
            reverb.reset();

            juce::AudioBuffer<float> input (2, 512);
            juce::AudioBuffer<float> output (2, 512);

            fillImpulse (input, 0.8f);
            reverb.process (input, output, 512);

            float levelAtFreeze = 0.0f;

            for (int block = 0; block < 40; ++block)
            {
                input.clear();
                reverb.process (input, output, 512);
                levelAtFreeze = blockRms (output, 512);
            }

            parameters.freeze = true;
            reverb.setParameters (parameters);

            float energyWhileFrozen = 0.0f;

            for (int block = 0; block < 120; ++block)
            {
                input.clear();
                reverb.process (input, output, 512);
                expect (allFinite (output, 512));

                // Ignore the first blocks while the freeze crossfade settles.
                if (block >= 20)
                    energyWhileFrozen = juce::jmax (energyWhileFrozen, blockRms (output, 512));
            }

            expect (energyWhileFrozen > levelAtFreeze * 0.25f,
                    "Freeze did not sustain the tail");

            parameters.freeze = false;
            reverb.setParameters (parameters);

            float energyAfterRelease = 0.0f;

            for (int block = 0; block < 240; ++block)
            {
                input.clear();
                reverb.process (input, output, 512);

                // Only look at the end of the release window.
                if (block >= 200)
                    energyAfterRelease = juce::jmax (energyAfterRelease, blockRms (output, 512));
            }

            expect (energyAfterRelease < energyWhileFrozen * 0.5f,
                    "Releasing freeze did not let the tail decay");
        }

        beginTest ("Extreme settings stay bounded at every supported sample rate and block size");

        constexpr std::array<double, 4> sampleRates { 44100.0, 48000.0, 88200.0, 96000.0 };
        constexpr std::array<int, 6> blockSizes { 32, 64, 128, 256, 512, 1024 };

        for (const auto sampleRate : sampleRates)
        {
            for (const auto blockSize : blockSizes)
            {
                DarkReverb reverb;
                reverb.prepare (sampleRate, blockSize);

                DarkReverb::Parameters parameters;
                parameters.sizeSeconds = 10.0f;
                parameters.damping = 0.0f;
                parameters.diffusion = 1.0f;
                parameters.modulationRateHz = 2.0f;
                parameters.modulationDepth = 1.0f;
                parameters.preDelayMs = 500.0f;
                parameters.width = 2.0f;
                parameters.freeze = false;
                reverb.setParameters (parameters);
                reverb.reset();

                juce::AudioBuffer<float> input (2, blockSize);
                juce::AudioBuffer<float> output (2, blockSize);

                for (int block = 0; block < 40; ++block)
                {
                    fillStereoSine (input, sampleRate);
                    reverb.process (input, output, blockSize);

                    expect (allFinite (output, blockSize));
                    expect (output.getMagnitude (0, 0, blockSize) <= 1.001f);
                    expect (output.getMagnitude (1, 0, blockSize) <= 1.001f);
                }

                parameters.freeze = true;
                reverb.setParameters (parameters);

                for (int block = 0; block < 40; ++block)
                {
                    input.clear();
                    reverb.process (input, output, blockSize);

                    expect (allFinite (output, blockSize));
                    expect (output.getMagnitude (0, 0, blockSize) <= 1.001f);
                    expect (output.getMagnitude (1, 0, blockSize) <= 1.001f);
                }
            }
        }

        beginTest ("Silence in produces silence out");

        {
            DarkReverb reverb;
            reverb.prepare (48000.0, 512);

            DarkReverb::Parameters parameters;
            parameters.sizeSeconds = 5.0f;
            reverb.setParameters (parameters);
            reverb.reset();

            juce::AudioBuffer<float> input (2, 512);
            juce::AudioBuffer<float> output (2, 512);
            input.clear();

            for (int block = 0; block < 10; ++block)
            {
                reverb.process (input, output, 512);
                expectEquals (output.getMagnitude (0, 0, 512), 0.0f);
                expectEquals (output.getMagnitude (1, 0, 512), 0.0f);
            }
        }

        beginTest ("Processor dry/wet at 0% is bit-transparent with the reverb engaged");

        {
            DarkGrainAudioProcessor processor;
            setParameter (processor, darkgrain::parameters::ids::dryWet, 0.0f);
            setParameter (processor, darkgrain::parameters::ids::reverbMix, 100.0f);
            setParameter (processor, darkgrain::parameters::ids::reverbSize, 6.0f);
            processor.prepareToPlay (48000.0, 512);

            juce::AudioBuffer<float> buffer (2, 512);
            fillStereoSine (buffer, 48000.0);

            juce::AudioBuffer<float> reference (2, 512);
            reference.makeCopyOf (buffer);

            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);

            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0; sample < 512; ++sample)
                    expectWithinAbsoluteError (buffer.getSample (channel, sample),
                                               reference.getSample (channel, sample),
                                               1.0e-6f);
        }

        beginTest ("Processor produces a reverb tail after the input stops");

        {
            DarkGrainAudioProcessor processor;
            setParameter (processor, darkgrain::parameters::ids::dryWet, 100.0f);
            setParameter (processor, darkgrain::parameters::ids::reverbMix, 100.0f);
            setParameter (processor, darkgrain::parameters::ids::reverbSize, 5.0f);
            setParameter (processor, darkgrain::parameters::ids::preDelay, 0.0f);
            setParameter (processor, darkgrain::parameters::ids::width, 100.0f);
            processor.prepareToPlay (48000.0, 512);

            juce::AudioBuffer<float> buffer (2, 512);
            fillImpulse (buffer);

            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);

            float tailEnergy = 0.0f;

            for (int block = 0; block < 80; ++block)
            {
                buffer.clear();
                processor.processBlock (buffer, midi);
                tailEnergy = juce::jmax (tailEnergy, blockRms (buffer, 512));
            }

            expect (tailEnergy > 0.0f, "No reverb tail was produced");
        }

        beginTest ("Processor bypass stays clean with the reverb engaged");

        {
            DarkGrainAudioProcessor processor;
            setParameter (processor, darkgrain::parameters::ids::bypass, 1.0f);
            setParameter (processor, darkgrain::parameters::ids::dryWet, 100.0f);
            setParameter (processor, darkgrain::parameters::ids::reverbMix, 100.0f);
            setParameter (processor, darkgrain::parameters::ids::inputGain, 12.0f);
            processor.prepareToPlay (48000.0, 512);

            juce::AudioBuffer<float> buffer (2, 512);
            fillStereoSine (buffer, 48000.0);

            juce::AudioBuffer<float> reference (2, 512);
            reference.makeCopyOf (buffer);

            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);

            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0; sample < 512; ++sample)
                    expectWithinAbsoluteError (buffer.getSample (channel, sample),
                                               reference.getSample (channel, sample),
                                               1.0e-6f);
        }

        beginTest ("Processor reports a non-zero tail length");

        {
            DarkGrainAudioProcessor processor;
            expect (processor.getTailLengthSeconds() > 0.0);
        }
    }
};

static DarkGrainReverbTests darkGrainReverbTests;
