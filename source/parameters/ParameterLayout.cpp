#include "parameters/ParameterLayout.h"

#include "parameters/ParameterIDs.h"

#include <memory>

namespace darkgrain::parameters
{
namespace
{
using ParameterLayout = juce::AudioProcessorValueTreeState::ParameterLayout;

std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id,
                                                      const juce::String& name,
                                                      juce::NormalisableRange<float> range,
                                                      float defaultValue,
                                                      const juce::String& label)
{
    const auto attributes = juce::AudioParameterFloatAttributes().withLabel (label);

    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 },
                                                        name,
                                                        std::move (range),
                                                        defaultValue,
                                                        attributes);
}

std::unique_ptr<juce::AudioParameterBool> makeBool (const char* id,
                                                    const juce::String& name,
                                                    bool defaultValue,
                                                    const juce::String& label)
{
    const auto attributes = juce::AudioParameterBoolAttributes().withLabel (label);

    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 },
                                                       name,
                                                       defaultValue,
                                                       attributes);
}

std::unique_ptr<juce::AudioParameterChoice> makeChoice (const char* id,
                                                        const juce::String& name,
                                                        juce::StringArray choices,
                                                        int defaultIndex)
{
    return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 },
                                                         name,
                                                         std::move (choices),
                                                         defaultIndex);
}
} // namespace

ParameterLayout createParameterLayout()
{
    ParameterLayout layout;

    // INPUT
    layout.add (makeFloat (ids::inputGain, "Input Gain", { -24.0f, 24.0f, 0.01f }, 0.0f, "dB"));

    auto highPassRange = juce::NormalisableRange<float> { 20.0f, 1000.0f, 0.1f };
    highPassRange.setSkewForCentre (100.0f);
    layout.add (makeFloat (ids::highPass, "High-Pass", highPassRange, 40.0f, "Hz"));

    // GRAINS
    auto grainSizeRange = juce::NormalisableRange<float> { 10.0f, 500.0f, 0.1f };
    grainSizeRange.setSkewForCentre (90.0f);
    layout.add (makeFloat (ids::grainSize, "Grain Size", grainSizeRange, 90.0f, "ms"));

    auto grainDensityRange = juce::NormalisableRange<float> { 1.0f, 64.0f, 0.1f };
    grainDensityRange.setSkewForCentre (12.0f);
    layout.add (makeFloat (ids::grainDensity, "Grain Density", grainDensityRange, 12.0f, "grains/s"));

    layout.add (makeFloat (ids::grainSpread, "Grain Spread", { 0.0f, 100.0f, 0.1f }, 35.0f, "%"));
    layout.add (makeFloat (ids::grainPitch, "Grain Pitch", { -24.0f, 24.0f, 0.01f }, 0.0f, "st"));
    layout.add (makeFloat (ids::pitchScatter, "Pitch Scatter", { 0.0f, 24.0f, 0.01f }, 0.0f, "st"));
    layout.add (makeFloat (ids::reverseAmount, "Reverse Amount", { 0.0f, 100.0f, 0.1f }, 20.0f, "%"));
    layout.add (makeFloat (ids::stereoSpread, "Stereo Spread", { 0.0f, 100.0f, 0.1f }, 45.0f, "%"));
    layout.add (makeBool (ids::freeze, "Freeze", false, "On/Off"));

    // PARTICLES
    layout.add (makeFloat (ids::particleDensity, "Particle Density", { 0.0f, 100.0f, 0.1f }, 35.0f, "%"));
    layout.add (makeFloat (ids::particleBrightness, "Particle Brightness", { 0.0f, 100.0f, 0.1f }, 40.0f, "%"));
    layout.add (makeFloat (ids::particleRandomness, "Particle Randomness", { 0.0f, 100.0f, 0.1f }, 50.0f, "%"));

    auto particleSizeRange = juce::NormalisableRange<float> { 10.0f, 500.0f, 0.1f };
    particleSizeRange.setSkewForCentre (90.0f);
    layout.add (makeFloat (ids::particleSize, "Particle Size", particleSizeRange, 70.0f, "ms"));

    layout.add (makeFloat (ids::particlePitch, "Particle Pitch", { -24.0f, 24.0f, 0.01f }, 0.0f, "st"));
    layout.add (makeFloat (ids::particleWidth, "Particle Width", { 0.0f, 100.0f, 0.1f }, 55.0f, "%"));
    layout.add (makeFloat (ids::particleFeedback, "Particle Feedback", { 0.0f, 100.0f, 0.1f }, 20.0f, "%"));
    layout.add (makeFloat (ids::particleMix, "Particle Mix", { 0.0f, 100.0f, 0.1f }, 30.0f, "%"));

    // SPACE
    auto reverbSizeRange = juce::NormalisableRange<float> { 0.1f, 10.0f, 0.01f };
    reverbSizeRange.setSkewForCentre (2.5f);
    layout.add (makeFloat (ids::reverbSize, "Reverb Size", reverbSizeRange, 2.5f, "s"));

    layout.add (makeFloat (ids::reverbDamping, "Reverb Damping", { 0.0f, 100.0f, 0.1f }, 55.0f, "%"));
    layout.add (makeFloat (ids::diffusion, "Diffusion", { 0.0f, 100.0f, 0.1f }, 60.0f, "%"));

    auto modulationRateRange = juce::NormalisableRange<float> { 0.01f, 2.0f, 0.001f };
    modulationRateRange.setSkewForCentre (0.15f);
    layout.add (makeFloat (ids::modulationRate, "Modulation Rate", modulationRateRange, 0.15f, "Hz"));

    layout.add (makeFloat (ids::modulationDepth, "Modulation Depth", { 0.0f, 100.0f, 0.1f }, 25.0f, "%"));

    auto preDelayRange = juce::NormalisableRange<float> { 0.0f, 500.0f, 0.1f };
    preDelayRange.setSkewForCentre (100.0f);
    layout.add (makeFloat (ids::preDelay, "Pre-Delay", preDelayRange, 35.0f, "ms"));

    layout.add (makeFloat (ids::reverbMix, "Reverb Mix", { 0.0f, 100.0f, 0.1f }, 35.0f, "%"));

    // OUTPUT
    layout.add (makeFloat (ids::toneDark, "Tone Dark", { 0.0f, 100.0f, 0.1f }, 45.0f, "%"));
    layout.add (makeFloat (ids::saturation, "Saturation", { 0.0f, 100.0f, 0.1f }, 15.0f, "%"));
    layout.add (makeFloat (ids::width, "Width", { 0.0f, 200.0f, 0.1f }, 120.0f, "%"));
    layout.add (makeFloat (ids::outputGain, "Output Gain", { -24.0f, 12.0f, 0.01f }, 0.0f, "dB"));
    layout.add (makeFloat (ids::dryWet, "Dry/Wet", { 0.0f, 100.0f, 0.1f }, 50.0f, "%"));

    // MUSICAL KEY
    layout.add (makeChoice (ids::rootKey,
                            "Root Key",
                            { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" },
                            0));

    layout.add (makeChoice (ids::scale,
                            "Scale",
                            { "Chromatic", "Minor", "Major", "Dorian", "Phrygian", "Lydian",
                              "Mixolydian", "Harmonic Minor", "Pentatonic Minor", "Pentatonic Major" },
                            1));

    // Internal host bypass. Kept in the layout so state, automation, and host
    // bypass all use the same parameter.
    layout.add (makeBool (ids::bypass, "Bypass", false, "Bypass"));

    return layout;
}
} // namespace darkgrain::parameters
