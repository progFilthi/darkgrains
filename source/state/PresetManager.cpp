#include "state/PresetManager.h"

#include "parameters/ParameterIDs.h"

#include <memory>

namespace darkgrain::state
{
namespace
{
namespace ids = darkgrain::parameters::ids;
} // namespace

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& stateToUse)
    : apvts (stateToUse)
{
    for (auto* parameter : apvts.processor.getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
        {
            defaultValues.push_back ({ ranged->getParameterID(),
                                       ranged->convertFrom0to1 (ranged->getDefaultValue()) });
        }
    }

    factoryPresets.push_back ({ "Init", {} });

    factoryPresets.push_back ({ "Cinematic Dark",
        {
            { ids::grainSize, 150.0f }, { ids::grainDensity, 9.0f }, { ids::grainSpread, 55.0f },
            { ids::pitchScatter, 5.0f }, { ids::reverseAmount, 30.0f }, { ids::stereoSpread, 60.0f },
            { ids::particleDensity, 30.0f }, { ids::particleBrightness, 35.0f },
            { ids::particleSize, 90.0f }, { ids::particleMix, 22.0f },
            { ids::reverbSize, 6.5f }, { ids::reverbDamping, 72.0f }, { ids::diffusion, 70.0f },
            { ids::preDelay, 55.0f }, { ids::reverbMix, 50.0f },
            { ids::toneDark, 62.0f }, { ids::saturation, 18.0f }, { ids::width, 135.0f },
            { ids::dryWet, 60.0f }
        } });

    factoryPresets.push_back ({ "Frozen Choir",
        {
            { ids::freeze, 1.0f },
            { ids::grainSize, 260.0f }, { ids::grainDensity, 7.0f }, { ids::grainSpread, 85.0f },
            { ids::grainPitch, 0.0f }, { ids::pitchScatter, 2.0f }, { ids::reverseAmount, 15.0f },
            { ids::stereoSpread, 70.0f },
            { ids::particleDensity, 20.0f }, { ids::particleBrightness, 30.0f },
            { ids::particleMix, 18.0f },
            { ids::reverbSize, 8.0f }, { ids::reverbDamping, 65.0f }, { ids::diffusion, 80.0f },
            { ids::modulationRate, 0.08f }, { ids::modulationDepth, 35.0f }, { ids::preDelay, 80.0f },
            { ids::reverbMix, 55.0f }, { ids::toneDark, 58.0f }, { ids::width, 150.0f },
            { ids::dryWet, 65.0f }
        } });

    factoryPresets.push_back ({ "Granular Storm",
        {
            { ids::grainSize, 48.0f }, { ids::grainDensity, 38.0f }, { ids::grainSpread, 92.0f },
            { ids::pitchScatter, 14.0f }, { ids::reverseAmount, 45.0f }, { ids::stereoSpread, 85.0f },
            { ids::particleDensity, 55.0f }, { ids::particleBrightness, 55.0f },
            { ids::particleRandomness, 70.0f }, { ids::particleMix, 30.0f },
            { ids::reverbSize, 3.2f }, { ids::reverbDamping, 45.0f }, { ids::diffusion, 55.0f },
            { ids::reverbMix, 30.0f },
            { ids::toneDark, 35.0f }, { ids::saturation, 28.0f }, { ids::width, 160.0f },
            { ids::dryWet, 68.0f }
        } });

    factoryPresets.push_back ({ "Dust Particles",
        {
            { ids::grainSize, 30.0f }, { ids::grainDensity, 6.0f }, { ids::grainSpread, 70.0f },
            { ids::pitchScatter, 8.0f },
            { ids::particleDensity, 72.0f }, { ids::particleSize, 45.0f },
            { ids::particleBrightness, 72.0f }, { ids::particlePitch, -12.0f },
            { ids::particleWidth, 85.0f }, { ids::particleRandomness, 82.0f },
            { ids::particleFeedback, 35.0f }, { ids::particleMix, 65.0f },
            { ids::reverbSize, 4.5f }, { ids::reverbDamping, 50.0f }, { ids::diffusion, 65.0f },
            { ids::preDelay, 25.0f }, { ids::reverbMix, 40.0f },
            { ids::toneDark, 42.0f }, { ids::width, 145.0f }, { ids::dryWet, 55.0f }
        } });

    factoryPresets.push_back ({ "Deep Space",
        {
            { ids::grainSize, 220.0f }, { ids::grainDensity, 5.0f }, { ids::grainSpread, 90.0f },
            { ids::pitchScatter, 10.0f }, { ids::reverseAmount, 35.0f }, { ids::stereoSpread, 80.0f },
            { ids::particleDensity, 25.0f }, { ids::particleBrightness, 25.0f },
            { ids::particleMix, 20.0f },
            { ids::reverbSize, 9.5f }, { ids::reverbDamping, 78.0f }, { ids::diffusion, 90.0f },
            { ids::modulationRate, 0.04f }, { ids::modulationDepth, 45.0f }, { ids::preDelay, 140.0f },
            { ids::reverbMix, 65.0f }, { ids::toneDark, 74.0f }, { ids::saturation, 12.0f },
            { ids::width, 170.0f }, { ids::dryWet, 62.0f }
        } });
}

juce::StringArray PresetManager::getFactoryPresetNames() const
{
    juce::StringArray names;

    for (const auto& preset : factoryPresets)
        names.add (preset.name);

    return names;
}

juce::StringArray PresetManager::getUserPresetNames() const
{
    juce::StringArray names;

    const auto directory = getPresetDirectory();

    if (! directory.isDirectory())
        return names;

    for (const auto& file : directory.findChildFiles (juce::File::findFiles, false, "*.xml"))
        names.add (file.getFileNameWithoutExtension());

    names.sort (true);

    return names;
}

juce::StringArray PresetManager::getAllPresetNames() const
{
    auto names = getFactoryPresetNames();
    names.addArray (getUserPresetNames());
    return names;
}

bool PresetManager::isFactoryPreset (const juce::String& name) const
{
    for (const auto& preset : factoryPresets)
        if (preset.name == name)
            return true;

    return false;
}

void PresetManager::setParameterValue (const juce::String& id, float value)
{
    if (auto* parameter = apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

void PresetManager::resetToDefaults()
{
    for (const auto& value : defaultValues)
        setParameterValue (value.id, value.value);
}

void PresetManager::applyValues (const std::vector<Value>& values)
{
    resetToDefaults();

    for (const auto& value : values)
        setParameterValue (value.id, value.value);
}

bool PresetManager::applyPreset (const juce::String& name)
{
    for (const auto& preset : factoryPresets)
    {
        if (preset.name == name)
        {
            applyValues (preset.values);
            currentPresetName = preset.name;
            return true;
        }
    }

    return applyUserPreset (name);
}

bool PresetManager::applyUserPreset (const juce::String& name)
{
    const auto file = getPresetDirectory()
                          .getChildFile (juce::File::createLegalFileName (name) + ".xml");

    if (! file.existsAsFile())
        return false;

    const std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (file));

    if (xml == nullptr)
        return false;

    resetToDefaults();

    for (auto* child = xml->getFirstChildElement(); child != nullptr; child = child->getNextElement())
    {
        if (! child->hasTagName ("PARAM"))
            continue;

        const auto id = child->getStringAttribute ("id");

        if (id.isNotEmpty())
            setParameterValue (id, static_cast<float> (child->getDoubleAttribute ("value", 0.0)));
    }

    currentPresetName = name;

    return true;
}

bool PresetManager::saveUserPreset (const juce::String& name)
{
    const auto trimmed = name.trim();

    if (trimmed.isEmpty())
        return false;

    const auto directory = getPresetDirectory();

    if (! directory.createDirectory().wasOk() && ! directory.isDirectory())
        return false;

    const auto file = directory.getChildFile (juce::File::createLegalFileName (trimmed) + ".xml");
    const auto state = apvts.copyState();
    const auto xml = state.createXml();

    if (xml == nullptr)
        return false;

    if (! file.replaceWithText (xml->toString()))
        return false;

    currentPresetName = trimmed;

    return true;
}

juce::File PresetManager::getPresetDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("DarkGrain")
        .getChildFile ("Presets");
}
} // namespace darkgrain::state
