#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <vector>

class DarkGrainAudioProcessor;

/**
    Full DarkGrain interface.

    A single dark window with input, grain, particle, space and output sections.
    Every control is attached to the APVTS, the preset bar can save and recall
    user presets, and a stereo level meter plus a freeze indicator give quick
    feedback while playing.
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

    DarkGrainAudioProcessor& processorRef;
    std::unique_ptr<DarkGrainLookAndFeel> lookAndFeel;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label statusLabel;

    juce::ComboBox presetBox;
    juce::TextButton savePresetButton { "Save" };
    juce::ToggleButton freezeButton { "Freeze" };
    juce::ToggleButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

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

    bool wasFrozen = false;
    float displayedInputLevel = 0.0f;
    float displayedOutputLevel = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DarkGrainAudioProcessorEditor)
};
