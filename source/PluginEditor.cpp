#include "PluginEditor.h"

#include "PluginProcessor.h"
#include "parameters/ParameterIDs.h"

namespace
{
const auto backgroundColour = juce::Colour::fromRGB (13, 14, 17);
const auto panelColour = juce::Colour::fromRGB (24, 26, 31);
const auto dividerColour = juce::Colour::fromRGB (44, 47, 54);
const auto trackColour = juce::Colour::fromRGB (48, 51, 58);
const auto accentColour = juce::Colour::fromRGB (198, 158, 106);
const auto accentBrightColour = juce::Colour::fromRGB (228, 191, 139);
const auto textPrimaryColour = juce::Colour::fromRGB (235, 232, 226);
const auto textDimColour = juce::Colour::fromRGB (156, 155, 152);

juce::Font makeFont (float height, bool bold = false)
{
    const auto style = bold ? juce::Font::bold : juce::Font::plain;
    return juce::Font (juce::FontOptions (height, style));
}
} // namespace

class DarkGrainAudioProcessorEditor::MeterComponent final : public juce::Component
{
public:
    void setLevels (float newInputLevel, float newOutputLevel)
    {
        inputLevel = newInputLevel;
        outputLevel = newOutputLevel;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto rows = getLocalBounds();

        drawRow (g, rows.removeFromTop (rows.getHeight() / 2), "IN", inputLevel);
        drawRow (g, rows, "OUT", outputLevel);
    }

private:
    static void drawRow (juce::Graphics& g,
                         juce::Rectangle<int> row,
                         const juce::String& label,
                         float linearLevel)
    {
        g.setFont (makeFont (10.0f));
        g.setColour (textDimColour);
        g.drawText (label, row.removeFromLeft (26), juce::Justification::centredLeft, false);

        const auto track = row.reduced (0, 6).toFloat();

        g.setColour (trackColour);
        g.fillRoundedRectangle (track, 2.0f);

        const auto decibels = juce::Decibels::gainToDecibels (linearLevel, -60.0f);
        const auto normalised = juce::jlimit (0.0f, 1.0f, (decibels + 60.0f) / 60.0f);

        if (normalised > 0.0f)
        {
            g.setColour (accentColour);
            g.fillRoundedRectangle (track.withWidth (track.getWidth() * normalised), 2.0f);
        }
    }

    float inputLevel = 0.0f;
    float outputLevel = 0.0f;
};

DarkGrainAudioProcessorEditor::DarkGrainAudioProcessorEditor (DarkGrainAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p)
{
    setLookAndFeel (&lookAndFeel);

    lookAndFeel.setColour (juce::Label::textColourId, textPrimaryColour);
    lookAndFeel.setColour (juce::Slider::backgroundColourId, trackColour);
    lookAndFeel.setColour (juce::Slider::trackColourId, accentColour);
    lookAndFeel.setColour (juce::Slider::thumbColourId, accentBrightColour);
    lookAndFeel.setColour (juce::Slider::textBoxTextColourId, textPrimaryColour);
    lookAndFeel.setColour (juce::Slider::textBoxBackgroundColourId, panelColour);
    lookAndFeel.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    lookAndFeel.setColour (juce::ToggleButton::textColourId, textPrimaryColour);
    lookAndFeel.setColour (juce::ToggleButton::tickColourId, accentColour);
    lookAndFeel.setColour (juce::ToggleButton::tickDisabledColourId, trackColour);

    addAndMakeVisible (titleLabel);
    titleLabel.setText ("DARKGRAIN", juce::dontSendNotification);
    titleLabel.setFont (makeFont (26.0f, true));
    titleLabel.setColour (juce::Label::textColourId, textPrimaryColour);
    titleLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (subtitleLabel);
    subtitleLabel.setText ("Granular texture & dark reverb", juce::dontSendNotification);
    subtitleLabel.setFont (makeFont (12.0f));
    subtitleLabel.setColour (juce::Label::textColourId, textDimColour);
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (presetLabel);
    presetLabel.setText ("Preset: Init", juce::dontSendNotification);
    presetLabel.setFont (makeFont (12.0f));
    presetLabel.setColour (juce::Label::textColourId, textDimColour);
    presetLabel.setJustificationType (juce::Justification::centredRight);

    addAndMakeVisible (inputSectionLabel);
    inputSectionLabel.setText ("INPUT", juce::dontSendNotification);
    inputSectionLabel.setFont (makeFont (11.0f, true));
    inputSectionLabel.setColour (juce::Label::textColourId, accentColour);

    addAndMakeVisible (outputSectionLabel);
    outputSectionLabel.setText ("OUTPUT", juce::dontSendNotification);
    outputSectionLabel.setFont (makeFont (11.0f, true));
    outputSectionLabel.setColour (juce::Label::textColourId, accentColour);

    addAndMakeVisible (inputGainLabel);
    inputGainLabel.setText ("Gain", juce::dontSendNotification);
    inputGainLabel.setFont (makeFont (12.0f));
    inputGainLabel.setColour (juce::Label::textColourId, textDimColour);

    addAndMakeVisible (outputGainLabel);
    outputGainLabel.setText ("Gain", juce::dontSendNotification);
    outputGainLabel.setFont (makeFont (12.0f));
    outputGainLabel.setColour (juce::Label::textColourId, textDimColour);

    const auto configureSlider = [this] (juce::Slider& slider)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 76, 22);
        slider.setPopupDisplayEnabled (false, false, this);
        addAndMakeVisible (slider);
    };

    configureSlider (inputGainSlider);
    configureSlider (outputGainSlider);

    addAndMakeVisible (bypassButton);
    bypassButton.setButtonText ("Bypass");

    meter = std::make_unique<MeterComponent>();
    addAndMakeVisible (*meter);

    inputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, darkgrain::parameters::ids::inputGain, inputGainSlider);

    outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, darkgrain::parameters::ids::outputGain, outputGainSlider);

    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.apvts, darkgrain::parameters::ids::bypass, bypassButton);

    setOpaque (true);
    setResizable (false, false);
    setSize (620, 300);
    startTimerHz (30);
}

DarkGrainAudioProcessorEditor::~DarkGrainAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DarkGrainAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (backgroundColour);

    g.setColour (accentColour.withAlpha (0.65f));
    g.fillRect (0, 0, getWidth(), 2);

    g.setColour (panelColour);
    g.fillRoundedRectangle (inputPanelBounds.toFloat(), 8.0f);
    g.fillRoundedRectangle (outputPanelBounds.toFloat(), 8.0f);

    g.setColour (dividerColour);
    g.fillRect (footerBounds.getX(), footerBounds.getY(), footerBounds.getWidth(), 1);

    g.setFont (makeFont (10.5f));
    g.setColour (textDimColour);
    g.drawText ("Stereo passthrough  |  0 samples latency",
                footerBounds.reduced (0, 1),
                juce::Justification::centredLeft,
                false);
    g.drawText ("v1.0.0",
                footerBounds.reduced (0, 1),
                juce::Justification::centredRight,
                false);
}

void DarkGrainAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (18);

    area.removeFromTop (2);

    auto header = area.removeFromTop (52);

    auto titleArea = header.removeFromLeft (320);
    titleLabel.setBounds (titleArea.removeFromTop (30));
    subtitleLabel.setBounds (titleArea.removeFromTop (18));

    presetLabel.setBounds (header.removeFromRight (170).removeFromTop (30));

    footerBounds = area.removeFromBottom (20);
    area.removeFromTop (6);

    constexpr int gap = 14;
    const auto panelWidth = (area.getWidth() - gap) / 2;

    inputPanelBounds = area.removeFromLeft (panelWidth);
    area.removeFromLeft (gap);
    outputPanelBounds = area;

    auto inputInner = inputPanelBounds.reduced (16);
    inputSectionLabel.setBounds (inputInner.removeFromTop (20));
    inputInner.removeFromTop (12);
    inputGainLabel.setBounds (inputInner.removeFromTop (18));
    inputInner.removeFromTop (4);
    inputGainSlider.setBounds (inputInner.removeFromTop (34));

    auto outputInner = outputPanelBounds.reduced (16);
    outputSectionLabel.setBounds (outputInner.removeFromTop (20));
    outputInner.removeFromTop (12);
    outputGainLabel.setBounds (outputInner.removeFromTop (18));
    outputInner.removeFromTop (4);
    outputGainSlider.setBounds (outputInner.removeFromTop (34));
    outputInner.removeFromTop (10);
    bypassButton.setBounds (outputInner.removeFromTop (28).removeFromLeft (130));
    outputInner.removeFromTop (6);

    if (meter != nullptr)
        meter->setBounds (outputInner);
}

void DarkGrainAudioProcessorEditor::timerCallback()
{
    const auto input = processorRef.getInputMeterLevel();
    const auto output = processorRef.getOutputMeterLevel();

    displayedInputLevel = input > displayedInputLevel ? input : displayedInputLevel * 0.82f;
    displayedOutputLevel = output > displayedOutputLevel ? output : displayedOutputLevel * 0.82f;

    if (meter != nullptr)
        meter->setLevels (displayedInputLevel, displayedOutputLevel);
}
