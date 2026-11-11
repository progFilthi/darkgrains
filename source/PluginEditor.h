#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <vector>

class DarkGrainAudioProcessor;

/**
    DarkGrain's editor.

    The default view is deliberately minimal: a living ember field with two
    macro knobs (GRAIN and SPACE), the project key, presets, freeze and bypass.
    All detailed controls live behind the ADVANCED button so the plugin feels
    like an instrument rather than a mixing console.
*/
class DarkGrainAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                            private juce::Timer
{
public:
    explicit DarkGrainAudioProcessorEditor (DarkGrainAudioProcessor&);
    ~DarkGrainAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class MeterComponent;
    class DarkGrainLookAndFeel;
    class AnimatedBackground;

    struct KnobControl
    {
        juce::Label name;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Panel
    {
        juce::String title;
        juce::Rectangle<int> bounds;
        juce::Rectangle<int> content;
        std::vector<KnobControl*> knobs;
        int columns = 1;
    };

    KnobControl& addKnob (const juce::String& parameterID, const juce::String& displayName);
    Panel& addPanel (const juce::String& title, int columns);
    void layoutPanel (Panel& panel, juce::Rectangle<int> bounds);
    void layoutKnobs (Panel& panel, juce::Rectangle<int> content);

    void timerCallback() override;

    void rebuildPresetList();
    void applyPresetFromBox();
    void savePresetFromBox();
    void setStatus (const juce::String& message);

    void setAdvancedVisible (bool shouldShowAdvanced);
    void syncMacroKnobsFromState();
    void applyGrainMacro();
    void applySpaceMacro();

    void setParameterValue (const char* id, float value);
    float getParameterValue (const char* id) const;

    DarkGrainAudioProcessor& processorRef;
    std::unique_ptr<DarkGrainLookAndFeel> lookAndFeel;
    std::unique_ptr<AnimatedBackground> animatedBackground;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label statusLabel;
    juce::Label keyLabel;
    juce::Label freezeIndicatorLabel;

    juce::ComboBox presetBox;
    juce::ComboBox rootKeyBox;
    juce::ComboBox scaleBox;
    juce::TextButton savePresetButton { "Save" };
    juce::TextButton advancedButton { "ADVANCED" };
    juce::ToggleButton freezeButton { "Freeze" };
    juce::ToggleButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rootKeyAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAttachment;

    juce::Label grainMacroName;
    juce::Label spaceMacroName;
    juce::Slider grainMacroSlider;
    juce::Slider spaceMacroSlider;

    std::vector<std::unique_ptr<KnobControl>> knobs;
    std::vector<std::unique_ptr<Panel>> panels;
    juce::StringArray presetNames;

    Panel* inputPanel = nullptr;
    Panel* grainsPanel = nullptr;
    Panel* particlesPanel = nullptr;
    Panel* spacePanel = nullptr;
    Panel* outputPanel = nullptr;

    std::unique_ptr<MeterComponent> meter;

    juce::Rectangle<int> headerBounds;
    juce::Rectangle<int> footerBounds;
    juce::Rectangle<int> contentBounds;

    bool advancedVisible = false;
    bool suppressMacroUpdates = false;
    bool wasFrozen = false;
    juce::String displayedKeyText;
    float displayedInputLevel = 0.0f;
    float displayedOutputLevel = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DarkGrainAudioProcessorEditor)
};
