#include "PluginProcessor.h"
#include "dsp/GrainEngine.h"
#include "dsp/ParticleEngine.h"
#include "parameters/ParameterIDs.h"

#include <array>
#include <cmath>

namespace
{
void setParameter (DarkGrainAudioProcessor& processor, const char* id, float value)
{
    if (auto* parameter = processor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

void fillSine (juce::AudioBuffer<float>& buffer, double sampleRate, int firstSample)
{
    const auto increment = juce::MathConstants<double>::twoPi * 220.0 / sampleRate;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto value = 0.4f * static_cast<float> (std::sin (increment * (firstSample + sample)));

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample (channel, sample, value);
    }
}

void fillNoise (juce::AudioBuffer<float>& buffer, juce::Random& random)
{
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto value = random.nextFloat() * 0.6f - 0.3f;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample (channel, sample, value);
    }
}

bool allFinite (const juce::AudioBuffer<float>& buffer, int numSamples)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < numSamples; ++sample)
            if (! std::isfinite (buffer.getSample (channel, sample)))
                return false;

    return true;
}

float peak (const juce::AudioBuffer<float>& buffer, int numSamples)
{
    auto maximum = 0.0f;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        maximum = juce::jmax (maximum, buffer.getMagnitude (channel, 0, numSamples));

    return maximum;
}
} // namespace

class DarkGrainGrainEngineTests final : public juce::UnitTest
{
public:
    DarkGrainGrainEngineTests()
        : juce::UnitTest ("Grain engine", "DarkGrain")
    {
    }

    void runTest() override
    {
        using namespace darkgrain::dsp;

        beginTest ("Circular buffer wraps and interpolates safely");

        {
            GrainBuffer buffer;
            buffer.prepare (48000.0);

            const auto length = buffer.getLength();
            expect (length >= 192000);

            juce::AudioBuffer<float> input (2, length);

            for (int sample = 0; sample < length; ++sample)
            {
                input.setSample (0, sample, 0.25f);
                input.setSample (1, sample, -0.25f);
            }

            buffer.write (input, length);

            expectWithinAbsoluteError (buffer.read (0, 0.0), 0.25f, 1.0e-6f);
            expectWithinAbsoluteError (buffer.read (0, 0.5), 0.25f, 1.0e-6f);
            expectWithinAbsoluteError (buffer.read (0, static_cast<double> (length) - 1.0), 0.25f, 1.0e-6f);
            expectWithinAbsoluteError (buffer.read (0, static_cast<double> (length) + 3.7), 0.25f, 1.0e-6f);
            expectWithinAbsoluteError (buffer.read (0, -2.0), 0.25f, 1.0e-6f);
            expectWithinAbsoluteError (buffer.read (1, 1234.25), -0.25f, 1.0e-6f);

            expectEquals (buffer.getAvailableHistory(), length);
        }

        beginTest ("Freezing stops the write head and recycles material");

        {
            GrainBuffer buffer;
            buffer.prepare (48000.0);

            juce::AudioBuffer<float> input (2, 4096);
            juce::Random random (0x1234567);

            for (int block = 0; block < 4; ++block)
            {
                fillNoise (input, random);
                buffer.write (input, input.getNumSamples());
            }

            buffer.setFrozen (true);

            for (int block = 0; block < 40; ++block)
                buffer.write (input, input.getNumSamples());

            const auto frozenHistory = buffer.getAvailableHistory();
            const auto frozenWritePosition = buffer.getWritePosition();

            for (int block = 0; block < 40; ++block)
                buffer.write (input, input.getNumSamples());

            expectEquals (buffer.getAvailableHistory(), frozenHistory);
            expectEquals (buffer.getWritePosition(), frozenWritePosition);

            buffer.setFrozen (false);

            for (int block = 0; block < 10; ++block)
                buffer.write (input, input.getNumSamples());

            expect (buffer.getAvailableHistory() > frozenHistory);
        }

        beginTest ("Grain engine remains bounded at extreme settings");

        {
            GrainEngine engine;
            engine.prepare (48000.0);

            GrainParameters parameters;
            parameters.sizeMs = 500.0f;
            parameters.density = 64.0f;
            parameters.spread = 1.0f;
            parameters.pitchSemitones = 24.0f;
            parameters.pitchScatterSemitones = 24.0f;
            parameters.reverseAmount = 1.0f;
            parameters.stereoSpread = 1.0f;
            parameters.freeze = false;
            engine.setParameters (parameters);

            juce::AudioBuffer<float> input (2, 512);
            juce::AudioBuffer<float> output (2, 512);
            juce::Random random (0x7654321);

            for (int block = 0; block < 400; ++block)
            {
                fillNoise (input, random);

                engine.process (input, output, input.getNumSamples());

                expect (allFinite (output, input.getNumSamples()));
                expect (peak (output, input.getNumSamples()) < 8.0f);
            }
        }

        beginTest ("Grain engine freeze holds energy after the input stops");

        {
            GrainEngine engine;
            engine.prepare (48000.0);

            GrainParameters parameters;
            parameters.sizeMs = 200.0f;
            parameters.density = 24.0f;
            parameters.spread = 0.8f;
            parameters.pitchScatterSemitones = 2.0f;
            parameters.reverseAmount = 0.4f;
            engine.setParameters (parameters);

            juce::AudioBuffer<float> input (2, 512);
            juce::AudioBuffer<float> output (2, 512);
            int samplePosition = 0;

            for (int block = 0; block < 300; ++block)
            {
                fillSine (input, 48000.0, samplePosition);
                samplePosition += input.getNumSamples();
                engine.process (input, output, input.getNumSamples());
            }

            parameters.freeze = true;
            engine.setParameters (parameters);

            input.clear();
            auto frozenPeak = 0.0f;

            for (int block = 0; block < 200; ++block)
            {
                engine.process (input, output, input.getNumSamples());
                expect (allFinite (output, input.getNumSamples()));
                frozenPeak = juce::jmax (frozenPeak, peak (output, input.getNumSamples()));
            }

            expect (frozenPeak > 0.001f);
            expect (frozenPeak < 8.0f);
        }

        beginTest ("Particle engine remains bounded at extreme settings");

        {
            GrainEngine history;
            history.prepare (48000.0);
            history.setParameters (GrainParameters {});

            juce::AudioBuffer<float> input (2, 512);
            juce::AudioBuffer<float> historyOutput (2, 512);
            juce::AudioBuffer<float> particleOutput (2, 512);

            juce::Random random (0xabcdef1);

            for (int block = 0; block < 200; ++block)
            {
                fillNoise (input, random);
                history.process (input, historyOutput, input.getNumSamples());
            }

            ParticleEngine particles;
            particles.prepare (48000.0);
            particles.setSource (&history.getBuffer());

            ParticleParameters parameters;
            parameters.density = 1.0f;
            parameters.sizeMs = 500.0f;
            parameters.brightness = 1.0f;
            parameters.pitchSemitones = 0.0f;
            parameters.panWidth = 1.0f;
            parameters.randomness = 1.0f;
            parameters.feedback = 1.0f;
            parameters.mix = 1.0f;
            particles.setParameters (parameters);

            for (int block = 0; block < 400; ++block)
            {
                particles.process (particleOutput, particleOutput.getNumSamples());

                expect (allFinite (particleOutput, particleOutput.getNumSamples()));
                expect (peak (particleOutput, particleOutput.getNumSamples()) < 4.0f);
            }
        }

        beginTest ("Full chain stays finite and limited under automation");

        {
            constexpr std::array<int, 6> blockSizes { 32, 64, 128, 256, 512, 1024 };

            for (const auto blockSize : blockSizes)
            {
                DarkGrainAudioProcessor processor;
                processor.prepareToPlay (48000.0, blockSize);

                juce::AudioBuffer<float> buffer (2, blockSize);
                juce::MidiBuffer midi;
                juce::Random random (0x5eed1234);
                int samplePosition = 0;

                for (int block = 0; block < 200; ++block)
                {
                    fillNoise (buffer, random);
                    samplePosition += buffer.getNumSamples();

                    const auto phase = static_cast<float> (block % 50) / 50.0f;

                    setParameter (processor, darkgrain::parameters::ids::inputGain, 12.0f * phase);
                    setParameter (processor, darkgrain::parameters::ids::highPass, 20.0f + 980.0f * phase);
                    setParameter (processor, darkgrain::parameters::ids::grainSize, 10.0f + 490.0f * phase);
                    setParameter (processor, darkgrain::parameters::ids::grainDensity, 1.0f + 63.0f * phase);
                    setParameter (processor, darkgrain::parameters::ids::grainSpread, 100.0f * phase);
                    setParameter (processor, darkgrain::parameters::ids::grainPitch, -24.0f + 48.0f * phase);
                    setParameter (processor, darkgrain::parameters::ids::pitchScatter, 24.0f * phase);
                    setParameter (processor, darkgrain::parameters::ids::reverseAmount, 100.0f * phase);
                    setParameter (processor, darkgrain::parameters::ids::stereoSpread, 100.0f * phase);
                    setParameter (processor, darkgrain::parameters::ids::freeze, phase > 0.5f ? 1.0f : 0.0f);
                    setParameter (processor, darkgrain::parameters::ids::particleDensity, 100.0f * phase);
                    setParameter (processor, darkgrain::parameters::ids::particleSize, 10.0f + 490.0f * phase);
                    setParameter (processor, darkgrain::parameters::ids::particleBrightness, 100.0f * phase);
                    setParameter (processor, darkgrain::parameters::ids::particlePitch, -24.0f);
                    setParameter (processor, darkgrain::parameters::ids::particleWidth, 100.0f * phase);
                    setParameter (processor, darkgrain::parameters::ids::particleRandomness, 100.0f * phase);
                    setParameter (processor, darkgrain::parameters::ids::particleFeedback, 100.0f * phase);
                    setParameter (processor, darkgrain::parameters::ids::particleMix, 100.0f * phase);
                    setParameter (processor, darkgrain::parameters::ids::reverbSize, 0.1f + 9.9f * phase);
                    setParameter (processor, darkgrain::parameters::ids::reverbDamping, 100.0f * phase);
                    setParameter (processor, darkgrain::parameters::ids::diffusion, 100.0f * phase);
                    setParameter (processor, darkgrain::parameters::ids::modulationRate, 0.01f + 1.99f * phase);
                    setParameter (processor, darkgrain::parameters::ids::modulationDepth, 100.0f * phase);
                    setParameter (processor, darkgrain::parameters::ids::preDelay, 500.0f * phase);
                    setParameter (processor, darkgrain::parameters::ids::reverbMix, 100.0f * phase);
                    setParameter (processor, darkgrain::parameters::ids::toneDark, 100.0f * phase);
                    setParameter (processor, darkgrain::parameters::ids::saturation, 100.0f * phase);
                    setParameter (processor, darkgrain::parameters::ids::width, 200.0f * phase);
                    setParameter (processor, darkgrain::parameters::ids::outputGain, 12.0f * phase);
                    setParameter (processor, darkgrain::parameters::ids::dryWet, 100.0f * phase);

                    processor.processBlock (buffer, midi);

                    expect (allFinite (buffer, buffer.getNumSamples()));
                    expect (peak (buffer, buffer.getNumSamples()) <= 1.01f);
                }

                juce::ignoreUnused (samplePosition);
            }
        }
    }
};

static DarkGrainGrainEngineTests darkGrainGrainEngineTests;
