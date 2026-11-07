#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

class DarkGrainAudioProcessor;

/**
    Lightweight native editor for the Milestone 1 foundation build.

    It exposes only the controls that are audible in this milestone (input gain,
    output gain, bypass) plus a stereo level meter. The full DarkGrain interface
    arrives with the UI milestone.
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

    void timerCallback() override;

    DarkGrainAudioProcessor& processorRef;
    juce::LookAndFeel_V4 lookAndFeel;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label presetLabel;
    juce::Label inputSectionLabel;
    juce::Label outputSectionLabel;
    juce::Label inputGainLabel;
    juce::Label outputGainLabel;

    juce::Slider inputGainSlider;
    juce::Slider outputGainSlider;
    juce::ToggleButton bypassButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    std::unique_ptr<MeterComponent> meter;

    juce::Rectangle<int> inputPanelBounds;
    juce::Rectangle<int> outputPanelBounds;
    juce::Rectangle<int> footerBounds;

    float displayedInputLevel = 0.0f;
    float displayedOutputLevel = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DarkGrainAudioProcessorEditor)
};
