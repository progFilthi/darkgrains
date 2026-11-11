#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <cmath>
#include <memory>

namespace
{
bool allFinite (const juce::AudioBuffer<float>& buffer, int numSamples)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < numSamples; ++sample)
            if (! std::isfinite (buffer.getSample (channel, sample)))
                return false;

    return true;
}

void fillNoise (juce::AudioBuffer<float>& buffer, juce::Random& random)
{
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto value = random.nextFloat() * 0.5f - 0.25f;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample (channel, sample, value);
    }
}
} // namespace

class DarkGrainEditorAndPresetTests final : public juce::UnitTest
{
public:
    DarkGrainEditorAndPresetTests()
        : juce::UnitTest ("Editor and presets", "DarkGrain")
    {
    }

    void runTest() override
    {
        beginTest ("Editor constructs, lays out, and paints");

        {
            DarkGrainAudioProcessor processor;
            processor.prepareToPlay (48000.0, 512);

            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
            expect (editor != nullptr);

            if (editor != nullptr)
            {
                editor->setSize (980, 620);

                juce::Image image (juce::Image::ARGB, 980, 620, true);
                juce::Graphics graphics (image);
                editor->paintEntireComponent (graphics, true);

                expect (true);
            }
        }

        beginTest ("Every factory preset applies and processes cleanly");

        {
            DarkGrainAudioProcessor processor;
            processor.prepareToPlay (48000.0, 512);

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            juce::Random random (0x1a2b3c4d);

            for (const auto& name : processor.getPresetManager().getFactoryPresetNames())
            {
                expect (processor.getPresetManager().applyPreset (name),
                        "Could not apply preset: " + name);

                for (auto* parameter : processor.getParameters())
                {
                    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                    {
                        const auto value = ranged->getValue();
                        expect (value >= -1.0e-4f && value <= 1.0001f,
                                name + " produced an out-of-range parameter: "
                                    + ranged->getParameterID());
                    }
                }

                fillNoise (buffer, random);
                processor.processBlock (buffer, midi);

                expect (allFinite (buffer, buffer.getNumSamples()),
                        "Non-finite output after preset: " + name);
            }
        }

        beginTest ("Rapid preset switching and sample-rate changes stay stable");

        {
            DarkGrainAudioProcessor processor;
            processor.prepareToPlay (44100.0, 256);

            juce::AudioBuffer<float> buffer (2, 256);
            juce::MidiBuffer midi;
            juce::Random random (0x5a5a1234);
            const auto names = processor.getPresetManager().getFactoryPresetNames();

            for (int block = 0; block < 256; ++block)
            {
                if (block % 16 == 0)
                {
                    const auto name = names[block / 16 % names.size()];
                    expect (processor.getPresetManager().applyPreset (name));
                }

                fillNoise (buffer, random);
                processor.processBlock (buffer, midi);

                expect (allFinite (buffer, buffer.getNumSamples()));
                expect (buffer.getMagnitude (0, 0, buffer.getNumSamples()) <= 1.01f);
                expect (buffer.getMagnitude (1, 0, buffer.getNumSamples()) <= 1.01f);

                if (block == 128)
                    processor.prepareToPlay (96000.0, 256);
            }
        }
    }
};

static DarkGrainEditorAndPresetTests darkGrainEditorAndPresetTests;
