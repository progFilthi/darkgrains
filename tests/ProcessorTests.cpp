#include "PluginProcessor.h"
#include "parameters/ParameterIDs.h"

#include <array>
#include <cmath>

namespace
{
constexpr float testFrequencyHz = 220.0f;
constexpr float testAmplitude = 0.25f;

void setParameter (DarkGrainAudioProcessor& processor, const char* id, float value)
{
    if (auto* parameter = processor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

void fillStereoSine (juce::AudioBuffer<float>& buffer, double sampleRate)
{
    const auto phaseIncrement = juce::MathConstants<double>::twoPi * testFrequencyHz / sampleRate;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto value = testAmplitude * static_cast<float> (std::sin (phaseIncrement * sample));

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample (channel, sample, value);
    }
}

void fillStereoSine (juce::AudioBuffer<float>& buffer, double sampleRate, int firstSample)
{
    const auto phaseIncrement = juce::MathConstants<double>::twoPi * testFrequencyHz / sampleRate;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto value = testAmplitude
                         * static_cast<float> (std::sin (phaseIncrement * (firstSample + sample)));

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample (channel, sample, value);
    }
}
} // namespace

class DarkGrainProcessorTests final : public juce::UnitTest
{
public:
    DarkGrainProcessorTests()
        : juce::UnitTest ("Audio passthrough, gain, and bypass", "DarkGrain")
    {
    }

    void runTest() override
    {
        beginTest ("Stereo passthrough is bit-transparent at supported sample rates and block sizes");

        constexpr std::array<double, 4> sampleRates { 44100.0, 48000.0, 88200.0, 96000.0 };
        constexpr std::array<int, 6> blockSizes { 32, 64, 128, 256, 512, 1024 };

        for (const auto sampleRate : sampleRates)
        {
            for (const auto blockSize : blockSizes)
            {
                DarkGrainAudioProcessor processor;
                setParameter (processor, darkgrain::parameters::ids::dryWet, 0.0f);
                processor.prepareToPlay (sampleRate, blockSize);

                juce::AudioBuffer<float> buffer (2, blockSize);
                fillStereoSine (buffer, sampleRate);

                juce::AudioBuffer<float> reference (2, blockSize);
                reference.makeCopyOf (buffer);

                juce::MidiBuffer midi;
                processor.processBlock (buffer, midi);

                for (int channel = 0; channel < 2; ++channel)
                {
                    for (int sample = 0; sample < blockSize; ++sample)
                    {
                        expectWithinAbsoluteError (buffer.getSample (channel, sample),
                                                   reference.getSample (channel, sample),
                                                   1.0e-6f);
                    }
                }

                expect (buffer.getMagnitude (0, 0, blockSize) > 0.0f);
            }
        }

        beginTest ("Input and output gain are applied");

        {
            DarkGrainAudioProcessor processor;
            setParameter (processor, darkgrain::parameters::ids::inputGain, 6.0f);
            setParameter (processor, darkgrain::parameters::ids::dryWet, 0.0f);
            processor.prepareToPlay (48000.0, 512);

            juce::AudioBuffer<float> buffer (2, 512);
            fillStereoSine (buffer, 48000.0);

            juce::AudioBuffer<float> reference (2, 512);
            reference.makeCopyOf (buffer);

            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);

            const auto expectedGain = juce::Decibels::decibelsToGain (6.0f);

            for (int channel = 0; channel < 2; ++channel)
            {
                for (int sample = 0; sample < 512; ++sample)
                {
                    expectWithinAbsoluteError (buffer.getSample (channel, sample),
                                               reference.getSample (channel, sample) * expectedGain,
                                               1.0e-5f);
                }
            }
        }

        {
            DarkGrainAudioProcessor processor;
            setParameter (processor, darkgrain::parameters::ids::outputGain, -6.0f);
            setParameter (processor, darkgrain::parameters::ids::dryWet, 0.0f);
            processor.prepareToPlay (48000.0, 512);

            juce::AudioBuffer<float> buffer (2, 512);
            fillStereoSine (buffer, 48000.0);

            juce::AudioBuffer<float> reference (2, 512);
            reference.makeCopyOf (buffer);

            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);

            const auto expectedGain = juce::Decibels::decibelsToGain (-6.0f);

            for (int channel = 0; channel < 2; ++channel)
            {
                for (int sample = 0; sample < 512; ++sample)
                {
                    expectWithinAbsoluteError (buffer.getSample (channel, sample),
                                               reference.getSample (channel, sample) * expectedGain,
                                               1.0e-5f);
                }
            }
        }

        beginTest ("Host bypass crossfades to clean input and back");

        {
            DarkGrainAudioProcessor processor;
            setParameter (processor, darkgrain::parameters::ids::inputGain, 12.0f);
            setParameter (processor, darkgrain::parameters::ids::dryWet, 0.0f);
            processor.prepareToPlay (48000.0, 512);

            constexpr int blockSize = 512;
            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;

            const auto drivenGain = juce::Decibels::decibelsToGain (12.0f);

            int samplePosition = 0;

            const auto processBlock = [&] (float expectedGain, bool checkOutput)
            {
                fillStereoSine (buffer, 48000.0, samplePosition);
                samplePosition += blockSize;

                juce::AudioBuffer<float> reference (2, blockSize);
                reference.makeCopyOf (buffer);

                processor.processBlock (buffer, midi);

                if (! checkOutput)
                    return;

                for (int channel = 0; channel < 2; ++channel)
                {
                    for (int sample = 0; sample < blockSize; ++sample)
                    {
                        expectWithinAbsoluteError (buffer.getSample (channel, sample),
                                                   reference.getSample (channel, sample) * expectedGain,
                                                   2.0e-4f);
                    }
                }
            };

            processBlock (drivenGain, true);

            setParameter (processor, darkgrain::parameters::ids::bypass, 1.0f);

            for (int i = 0; i < 7; ++i)
                processBlock (1.0f, false);

            processBlock (1.0f, true);

            setParameter (processor, darkgrain::parameters::ids::bypass, 0.0f);

            for (int i = 0; i < 7; ++i)
                processBlock (drivenGain, false);

            processBlock (drivenGain, true);
        }

        beginTest ("Silence, DC offset, and full-scale input remain finite");

        {
            DarkGrainAudioProcessor processor;
            setParameter (processor, darkgrain::parameters::ids::dryWet, 0.0f);
            processor.prepareToPlay (96000.0, 1024);

            juce::AudioBuffer<float> buffer (2, 1024);
            juce::MidiBuffer midi;

            buffer.clear();
            processor.processBlock (buffer, midi);
            expectEquals (buffer.getMagnitude (0, 0, 1024), 0.0f);
            expectEquals (buffer.getMagnitude (1, 0, 1024), 0.0f);

            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0; sample < 1024; ++sample)
                    buffer.setSample (channel, sample, 0.5f);

            processor.processBlock (buffer, midi);

            for (int channel = 0; channel < 2; ++channel)
            {
                for (int sample = 0; sample < 1024; ++sample)
                    expectWithinAbsoluteError (buffer.getSample (channel, sample), 0.5f, 1.0e-6f);
            }

            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0; sample < 1024; ++sample)
                    buffer.setSample (channel, sample, sample % 2 == 0 ? 1.0f : -1.0f);

            processor.processBlock (buffer, midi);
            expect (buffer.getMagnitude (0, 0, 1024) <= 1.0f);
            expect (buffer.getMagnitude (1, 0, 1024) <= 1.0f);
        }

        beginTest ("Repeated block processing remains stable");

        {
            DarkGrainAudioProcessor processor;
            setParameter (processor, darkgrain::parameters::ids::inputGain, 12.0f);
            setParameter (processor, darkgrain::parameters::ids::outputGain, 6.0f);
            processor.prepareToPlay (44100.0, 256);

            juce::AudioBuffer<float> buffer (2, 256);
            juce::MidiBuffer midi;
            int samplePosition = 0;

            for (int block = 0; block < 2000; ++block)
            {
                fillStereoSine (buffer, 44100.0, samplePosition);
                samplePosition += buffer.getNumSamples();
                processor.processBlock (buffer, midi);
            }

            expect (buffer.getMagnitude (0, 0, buffer.getNumSamples()) < 10.0f);
            expect (buffer.getMagnitude (1, 0, buffer.getNumSamples()) < 10.0f);
        }
    }
};

static DarkGrainProcessorTests darkGrainProcessorTests;
