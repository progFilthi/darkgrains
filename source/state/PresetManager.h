#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

namespace darkgrain::state
{
/**
    Factory and user preset handling.

    Presets are plain parameter snapshots. Applying one resets every parameter
    to its default and then applies the preset values through the host, so
    automation and the editor stay in sync. User presets live as XML files in
    the user application data directory.
*/
class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& stateToUse);

    juce::StringArray getFactoryPresetNames() const;
    juce::StringArray getUserPresetNames() const;
    juce::StringArray getAllPresetNames() const;

    bool isFactoryPreset (const juce::String& name) const;

    /** Applies a factory or user preset. Returns false if it was not found. */
    bool applyPreset (const juce::String& name);

    /** Saves the current parameter state as a user preset. */
    bool saveUserPreset (const juce::String& name);

    juce::String getCurrentPresetName() const { return currentPresetName; }

    static juce::File getPresetDirectory();

private:
    struct Value
    {
        juce::String id;
        float value = 0.0f;
    };

    struct Preset
    {
        juce::String name;
        std::vector<Value> values;
    };

    void applyValues (const std::vector<Value>& values);
    void resetToDefaults();
    void setParameterValue (const juce::String& id, float value);
    bool applyUserPreset (const juce::String& name);

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<Preset> factoryPresets;
    std::vector<Value> defaultValues;
    juce::String currentPresetName { "Init" };
};
} // namespace darkgrain::state
