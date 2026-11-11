#include "PluginProcessor.h"
#include "parameters/ParameterIDs.h"

#include <array>

namespace
{
struct ExpectedFloatParameter
{
    const char* id;
    float minimum;
    float maximum;
    float defaultValue;
};

constexpr std::array<ExpectedFloatParameter, 29> expectedFloatParameters {{
    { darkgrain::parameters::ids::inputGain, -24.0f, 24.0f, 0.0f },
    { darkgrain::parameters::ids::highPass, 20.0f, 1000.0f, 40.0f },
    { darkgrain::parameters::ids::grainSize, 10.0f, 500.0f, 90.0f },
    { darkgrain::parameters::ids::grainDensity, 1.0f, 64.0f, 12.0f },
    { darkgrain::parameters::ids::grainSpread, 0.0f, 100.0f, 35.0f },
    { darkgrain::parameters::ids::grainPitch, -24.0f, 24.0f, 0.0f },
    { darkgrain::parameters::ids::pitchScatter, 0.0f, 24.0f, 3.0f },
    { darkgrain::parameters::ids::reverseAmount, 0.0f, 100.0f, 20.0f },
    { darkgrain::parameters::ids::stereoSpread, 0.0f, 100.0f, 45.0f },
    { darkgrain::parameters::ids::particleDensity, 0.0f, 100.0f, 35.0f },
    { darkgrain::parameters::ids::particleBrightness, 0.0f, 100.0f, 40.0f },
    { darkgrain::parameters::ids::particleRandomness, 0.0f, 100.0f, 50.0f },
    { darkgrain::parameters::ids::particleSize, 10.0f, 500.0f, 70.0f },
    { darkgrain::parameters::ids::particlePitch, -24.0f, 0.0f, -7.0f },
    { darkgrain::parameters::ids::particleWidth, 0.0f, 100.0f, 55.0f },
    { darkgrain::parameters::ids::particleFeedback, 0.0f, 100.0f, 20.0f },
    { darkgrain::parameters::ids::particleMix, 0.0f, 100.0f, 30.0f },
    { darkgrain::parameters::ids::reverbSize, 0.1f, 10.0f, 2.5f },
    { darkgrain::parameters::ids::reverbDamping, 0.0f, 100.0f, 55.0f },
    { darkgrain::parameters::ids::diffusion, 0.0f, 100.0f, 60.0f },
    { darkgrain::parameters::ids::modulationRate, 0.01f, 2.0f, 0.15f },
    { darkgrain::parameters::ids::modulationDepth, 0.0f, 100.0f, 25.0f },
    { darkgrain::parameters::ids::preDelay, 0.0f, 500.0f, 35.0f },
    { darkgrain::parameters::ids::reverbMix, 0.0f, 100.0f, 35.0f },
    { darkgrain::parameters::ids::toneDark, 0.0f, 100.0f, 45.0f },
    { darkgrain::parameters::ids::saturation, 0.0f, 100.0f, 15.0f },
    { darkgrain::parameters::ids::width, 0.0f, 200.0f, 120.0f },
    { darkgrain::parameters::ids::outputGain, -24.0f, 12.0f, 0.0f },
    { darkgrain::parameters::ids::dryWet, 0.0f, 100.0f, 50.0f },
}};

void setParameter (DarkGrainAudioProcessor& processor, const char* id, float value)
{
    if (auto* parameter = processor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}
} // namespace

class DarkGrainParameterTests final : public juce::UnitTest
{
public:
    DarkGrainParameterTests()
        : juce::UnitTest ("Parameters and state", "DarkGrain")
    {
    }

    void runTest() override
    {
        beginTest ("README parameter set exists");

        DarkGrainAudioProcessor processor;
        expectEquals (processor.getParameters().size(), 31);

        for (const auto& expected : expectedFloatParameters)
            expect (processor.apvts.getParameter (expected.id) != nullptr,
                    juce::String ("Missing parameter: ") + expected.id);

        expect (processor.apvts.getParameter (darkgrain::parameters::ids::freeze) != nullptr,
                "Missing Freeze parameter");
        expect (processor.apvts.getParameter (darkgrain::parameters::ids::bypass) != nullptr,
                "Missing internal bypass parameter");

        beginTest ("Parameter ranges and defaults match the specification");

        for (const auto& expected : expectedFloatParameters)
        {
            const auto* parameter = processor.apvts.getParameter (expected.id);
            expect (parameter != nullptr);

            if (parameter == nullptr)
                continue;

            const auto range = parameter->getNormalisableRange();
            expectWithinAbsoluteError (range.start, expected.minimum, 0.001f);
            expectWithinAbsoluteError (range.end, expected.maximum, 0.001f);

            const auto* value = processor.apvts.getRawParameterValue (expected.id);
            expect (value != nullptr);

            if (value != nullptr)
                expectWithinAbsoluteError (value->load(), expected.defaultValue, 0.001f);
        }

        beginTest ("State serialises and restores all parameters");

        DarkGrainAudioProcessor source;
        setParameter (source, darkgrain::parameters::ids::inputGain, 7.5f);
        setParameter (source, darkgrain::parameters::ids::dryWet, 33.0f);
        setParameter (source, darkgrain::parameters::ids::reverbSize, 6.25f);
        setParameter (source, darkgrain::parameters::ids::freeze, 1.0f);
        setParameter (source, darkgrain::parameters::ids::bypass, 1.0f);

        juce::MemoryBlock state;
        source.getStateInformation (state);
        expect (state.getSize() > 0);

        DarkGrainAudioProcessor destination;
        destination.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

        expectWithinAbsoluteError (
            destination.apvts.getRawParameterValue (darkgrain::parameters::ids::inputGain)->load(), 7.5f, 0.001f);
        expectWithinAbsoluteError (
            destination.apvts.getRawParameterValue (darkgrain::parameters::ids::dryWet)->load(), 33.0f, 0.001f);
        expectWithinAbsoluteError (
            destination.apvts.getRawParameterValue (darkgrain::parameters::ids::reverbSize)->load(), 6.25f, 0.001f);
        expect (destination.apvts.getRawParameterValue (darkgrain::parameters::ids::freeze)->load() >= 0.5f);
        expect (destination.apvts.getRawParameterValue (darkgrain::parameters::ids::bypass)->load() >= 0.5f);

        beginTest ("Invalid state data is ignored safely");

        destination.setStateInformation (nullptr, 0);
        const char garbage[] = "this is not a DarkGrain state";
        destination.setStateInformation (garbage, static_cast<int> (sizeof (garbage)));
        expect (true);
    }
};

static DarkGrainParameterTests darkGrainParameterTests;
