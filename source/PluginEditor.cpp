#include "PluginEditor.h"

#include "PluginProcessor.h"
#include "parameters/ParameterIDs.h"

#include <cmath>

namespace
{
const auto backgroundColour = juce::Colour::fromRGB (13, 14, 17);
const auto panelColour = juce::Colour::fromRGB (24, 26, 31);
const auto knobColour = juce::Colour::fromRGB (33, 35, 41);
const auto dividerColour = juce::Colour::fromRGB (44, 47, 54);
const auto trackColour = juce::Colour::fromRGB (48, 51, 58);
const auto accentColour = juce::Colour::fromRGB (198, 158, 106);
const auto accentBrightColour = juce::Colour::fromRGB (228, 191, 139);
const auto textPrimaryColour = juce::Colour::fromRGB (235, 232, 226);
const auto textDimColour = juce::Colour::fromRGB (156, 155, 152);

juce::Font makeFont (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}
} // namespace

class DarkGrainAudioProcessorEditor::DarkGrainLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    DarkGrainLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, textPrimaryColour);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, textPrimaryColour);
        setColour (juce::ComboBox::backgroundColourId, panelColour);
        setColour (juce::ComboBox::textColourId, textPrimaryColour);
        setColour (juce::ComboBox::outlineColourId, dividerColour);
        setColour (juce::ComboBox::arrowColourId, accentColour);
        setColour (juce::PopupMenu::backgroundColourId, panelColour);
        setColour (juce::PopupMenu::textColourId, textPrimaryColour);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accentColour.withAlpha (0.25f));
        setColour (juce::PopupMenu::highlightedTextColourId, textPrimaryColour);
        setColour (juce::TextButton::buttonColourId, panelColour);
        setColour (juce::TextButton::textColourOffId, textPrimaryColour);
        setColour (juce::TextButton::textColourOnId, textPrimaryColour);
        setColour (juce::ToggleButton::textColourId, textPrimaryColour);
        setColour (juce::ToggleButton::tickColourId, accentColour);
        setColour (juce::ToggleButton::tickDisabledColourId, trackColour);
    }

    void drawRotarySlider (juce::Graphics& g,
                           int x,
                           int y,
                           int width,
                           int height,
                           float sliderPos,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centreX = bounds.getCentreX();
        const auto centreY = bounds.getCentreY();
        const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const auto lineWidth = juce::jmax (2.0f, radius * 0.16f);
        const auto arcRadius = radius - lineWidth * 0.5f;

        juce::Path backgroundArc;
        backgroundArc.addCentredArc (centreX, centreY, arcRadius, arcRadius, 0.0f,
                                     rotaryStartAngle, rotaryEndAngle, true);

        g.setColour (trackColour);
        g.strokePath (backgroundArc, juce::PathStrokeType (lineWidth,
                                                           juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded));

        if (slider.isEnabled())
        {
            juce::Path valueArc;
            valueArc.addCentredArc (centreX, centreY, arcRadius, arcRadius, 0.0f,
                                    rotaryStartAngle, angle, true);

            g.setColour (accentColour);
            g.strokePath (valueArc, juce::PathStrokeType (lineWidth,
                                                          juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
        }

        const auto knobRadius = radius * 0.6f;
        g.setColour (knobColour);
        g.fillEllipse (juce::Rectangle<float> (knobRadius * 2.0f, knobRadius * 2.0f)
                           .withCentre ({ centreX, centreY }));

        g.setColour (dividerColour);
        g.drawEllipse (juce::Rectangle<float> (knobRadius * 2.0f, knobRadius * 2.0f)
                           .withCentre ({ centreX, centreY }),
                       1.0f);

        const auto pointerWidth = juce::jmax (2.0f, radius * 0.12f);
        juce::Path pointer;
        pointer.addRoundedRectangle (-pointerWidth * 0.5f,
                                     -radius * 0.62f,
                                     pointerWidth,
                                     radius * 0.3f,
                                     pointerWidth * 0.5f);

        g.setColour (accentBrightColour);
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centreX, centreY));
    }
};

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
    lookAndFeel = std::make_unique<DarkGrainLookAndFeel>();
    setLookAndFeel (lookAndFeel.get());

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

    addAndMakeVisible (statusLabel);
    statusLabel.setText ("Ready", juce::dontSendNotification);
    statusLabel.setFont (makeFont (10.5f));
    statusLabel.setColour (juce::Label::textColourId, textDimColour);
    statusLabel.setJustificationType (juce::Justification::centredLeft);

    presetBox.setEditableText (true);
    presetBox.setJustificationType (juce::Justification::centredLeft);
    presetBox.setTooltip ("Select a preset, type a name, then Save");

    addAndMakeVisible (presetBox);
    addAndMakeVisible (savePresetButton);

    presetBox.onChange = [this] { applyPresetFromBox(); };
    savePresetButton.onClick = [this] { savePresetFromBox(); };

    addAndMakeVisible (freezeButton);
    bypassButton.setButtonText ("Bypass");
    addAndMakeVisible (bypassButton);

    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.apvts, darkgrain::parameters::ids::freeze, freezeButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.apvts, darkgrain::parameters::ids::bypass, bypassButton);

    using namespace darkgrain::parameters;

    inputPanel = &addPanel ("INPUT", 2);
    inputPanel->knobs.push_back (&addKnob (ids::inputGain, "Gain"));
    inputPanel->knobs.push_back (&addKnob (ids::highPass, "HP Filter"));

    grainsPanel = &addPanel ("GRAINS", 4);
    grainsPanel->knobs.push_back (&addKnob (ids::grainSize, "Size"));
    grainsPanel->knobs.push_back (&addKnob (ids::grainDensity, "Density"));
    grainsPanel->knobs.push_back (&addKnob (ids::grainSpread, "Spread"));
    grainsPanel->knobs.push_back (&addKnob (ids::grainPitch, "Pitch"));
    grainsPanel->knobs.push_back (&addKnob (ids::pitchScatter, "Scatter"));
    grainsPanel->knobs.push_back (&addKnob (ids::reverseAmount, "Reverse"));
    grainsPanel->knobs.push_back (&addKnob (ids::stereoSpread, "Stereo"));

    particlesPanel = &addPanel ("PARTICLES", 4);
    particlesPanel->knobs.push_back (&addKnob (ids::particleDensity, "Density"));
    particlesPanel->knobs.push_back (&addKnob (ids::particleSize, "Size"));
    particlesPanel->knobs.push_back (&addKnob (ids::particleBrightness, "Brightness"));
    particlesPanel->knobs.push_back (&addKnob (ids::particlePitch, "Pitch"));
    particlesPanel->knobs.push_back (&addKnob (ids::particleWidth, "Width"));
    particlesPanel->knobs.push_back (&addKnob (ids::particleRandomness, "Random"));
    particlesPanel->knobs.push_back (&addKnob (ids::particleFeedback, "Feedback"));
    particlesPanel->knobs.push_back (&addKnob (ids::particleMix, "Mix"));

    spacePanel = &addPanel ("SPACE", 4);
    spacePanel->knobs.push_back (&addKnob (ids::reverbSize, "Size"));
    spacePanel->knobs.push_back (&addKnob (ids::reverbDamping, "Damping"));
    spacePanel->knobs.push_back (&addKnob (ids::diffusion, "Diffusion"));
    spacePanel->knobs.push_back (&addKnob (ids::modulationRate, "Mod Rate"));
    spacePanel->knobs.push_back (&addKnob (ids::modulationDepth, "Mod Depth"));
    spacePanel->knobs.push_back (&addKnob (ids::preDelay, "Pre-Delay"));
    spacePanel->knobs.push_back (&addKnob (ids::reverbMix, "Reverb Mix"));

    outputPanel = &addPanel ("OUTPUT", 3);
    outputPanel->knobs.push_back (&addKnob (ids::toneDark, "Tone Dark"));
    outputPanel->knobs.push_back (&addKnob (ids::saturation, "Saturation"));
    outputPanel->knobs.push_back (&addKnob (ids::width, "Width"));
    outputPanel->knobs.push_back (&addKnob (ids::outputGain, "Output"));
    outputPanel->knobs.push_back (&addKnob (ids::dryWet, "Dry/Wet"));

    meter = std::make_unique<MeterComponent>();
    addAndMakeVisible (*meter);

    rebuildPresetList();

    setOpaque (true);
    setResizable (false, false);
    setSize (980, 620);
    startTimerHz (30);
}

DarkGrainAudioProcessorEditor::~DarkGrainAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

DarkGrainAudioProcessorEditor::KnobControl& DarkGrainAudioProcessorEditor::addKnob (
    const juce::String& parameterID,
    const juce::String& displayName)
{
    auto control = std::make_unique<KnobControl>();

    control->name.setText (displayName, juce::dontSendNotification);
    control->name.setFont (makeFont (11.0f));
    control->name.setColour (juce::Label::textColourId, textDimColour);
    control->name.setJustificationType (juce::Justification::centred);
    control->name.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (control->name);

    control->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    control->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
    control->slider.setPopupDisplayEnabled (true, true, this);
    addAndMakeVisible (control->slider);

    control->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, parameterID, control->slider);

    auto* raw = control.get();
    knobs.push_back (std::move (control));

    return *raw;
}

DarkGrainAudioProcessorEditor::Panel& DarkGrainAudioProcessorEditor::addPanel (
    const juce::String& title,
    int columns)
{
    auto panel = std::make_unique<Panel>();
    panel->title = title;
    panel->columns = columns;

    auto* raw = panel.get();
    panels.push_back (std::move (panel));

    return *raw;
}

void DarkGrainAudioProcessorEditor::layoutPanel (Panel& panel, juce::Rectangle<int> bounds)
{
    panel.bounds = bounds;
    panel.content = bounds.reduced (12);
    panel.content.removeFromTop (22);

    layoutKnobs (panel, panel.content);
}

void DarkGrainAudioProcessorEditor::layoutKnobs (Panel& panel, juce::Rectangle<int> content)
{
    const auto count = static_cast<int> (panel.knobs.size());

    if (count <= 0)
        return;

    const auto columns = juce::jmax (1, panel.columns);
    const auto rows = (count + columns - 1) / columns;
    const auto cellWidth = content.getWidth() / columns;
    const auto cellHeight = content.getHeight() / rows;

    for (int index = 0; index < count; ++index)
    {
        auto cell = juce::Rectangle<int> (content.getX() + (index % columns) * cellWidth,
                                          content.getY() + (index / columns) * cellHeight,
                                          cellWidth,
                                          cellHeight)
                        .reduced (3);

        auto& knob = *panel.knobs[static_cast<size_t> (index)];

        auto nameArea = cell.removeFromTop (13);
        knob.name.setBounds (nameArea);

        auto sliderArea = cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), 84),
                                                      juce::jmin (cell.getHeight(), 88));
        knob.slider.setBounds (sliderArea);
    }
}

void DarkGrainAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    headerBounds = area.removeFromTop (62);
    footerBounds = area.removeFromBottom (20);
    area.removeFromTop (6);

    constexpr int gap = 12;

    auto row1 = area.removeFromTop (252);
    area.removeFromTop (gap);
    auto row2 = area;

    auto header = headerBounds;
    auto titleArea = header.removeFromLeft (340);
    titleLabel.setBounds (titleArea.removeFromTop (30));
    subtitleLabel.setBounds (titleArea.removeFromTop (18));

    savePresetButton.setBounds (header.removeFromRight (64).withSizeKeepingCentre (64, 26));
    header.removeFromRight (8);
    presetBox.setBounds (header.removeFromRight (232).withSizeKeepingCentre (232, 26));

    auto inputBounds = row1.removeFromLeft (196);
    row1.removeFromLeft (gap);

    const auto grainsWidth = (row1.getWidth() - gap) / 2;
    auto grainsBounds = row1.removeFromLeft (grainsWidth);
    row1.removeFromLeft (gap);
    auto particlesBounds = row1;

    const auto outputWidth = 366;
    auto spaceBounds = row2.removeFromLeft (row2.getWidth() - outputWidth - gap);
    row2.removeFromLeft (gap);
    auto outputBounds = row2;

    layoutPanel (*inputPanel, inputBounds);
    layoutPanel (*grainsPanel, grainsBounds);
    layoutPanel (*particlesPanel, particlesBounds);
    layoutPanel (*spacePanel, spaceBounds);

    outputPanel->bounds = outputBounds;
    outputPanel->content = outputBounds.reduced (12);
    outputPanel->content.removeFromTop (22);

    auto knobContent = outputPanel->content;
    auto meterArea = knobContent.removeFromRight (58);
    layoutKnobs (*outputPanel, knobContent);
    meter->setBounds (meterArea.reduced (2, 8));

    auto grainsTitle = grainsPanel->bounds.reduced (14).withHeight (20);
    freezeButton.setBounds (grainsTitle.removeFromRight (76));

    auto outputTitle = outputPanel->bounds.reduced (14).withHeight (20);
    bypassButton.setBounds (outputTitle.removeFromRight (76));

    statusLabel.setBounds (footerBounds.removeFromLeft (740));
}

void DarkGrainAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (backgroundColour);

    g.setColour (accentColour.withAlpha (0.65f));
    g.fillRect (0, 0, getWidth(), 2);

    g.setColour (dividerColour);
    g.fillRect (headerBounds.getX(), headerBounds.getBottom(), headerBounds.getWidth(), 1);

    for (const auto& panelPtr : panels)
    {
        const auto& panel = *panelPtr;

        g.setColour (panelColour);
        g.fillRoundedRectangle (panel.bounds.toFloat(), 8.0f);

        if (panelPtr.get() == grainsPanel && wasFrozen)
        {
            g.setColour (accentColour);
            g.drawRoundedRectangle (panel.bounds.toFloat().reduced (0.75f), 8.0f, 1.6f);
        }

        g.setColour (accentColour);
        g.setFont (makeFont (10.5f, true));
        g.drawText (panel.title,
                    panel.bounds.reduced (14).withHeight (16),
                    juce::Justification::centredLeft,
                    false);
    }

    g.setColour (dividerColour);
    g.fillRect (footerBounds.getX(), footerBounds.getY(), footerBounds.getWidth(), 1);

    g.setFont (makeFont (10.5f));
    g.setColour (textDimColour);
    g.drawText ("0 samples latency  |  v1.0.0",
                footerBounds.reduced (0, 1),
                juce::Justification::centredRight,
                false);
}

void DarkGrainAudioProcessorEditor::timerCallback()
{
    const auto frozen = processorRef.isFrozen();

    if (frozen != wasFrozen)
    {
        wasFrozen = frozen;
        repaint();
    }

    const auto input = processorRef.getInputMeterLevel();
    const auto output = processorRef.getOutputMeterLevel();

    displayedInputLevel = input > displayedInputLevel ? input : displayedInputLevel * 0.82f;
    displayedOutputLevel = output > displayedOutputLevel ? output : displayedOutputLevel * 0.82f;

    if (meter != nullptr)
        meter->setLevels (displayedInputLevel, displayedOutputLevel);
}

void DarkGrainAudioProcessorEditor::rebuildPresetList()
{
    presetNames.clear();
    presetBox.clear (juce::dontSendNotification);

    int itemId = 1;

    presetBox.addSectionHeading ("Factory");

    for (const auto& name : processorRef.getPresetManager().getFactoryPresetNames())
    {
        presetNames.add (name);
        presetBox.addItem (name, itemId++);
    }

    const auto userPresets = processorRef.getPresetManager().getUserPresetNames();

    if (! userPresets.isEmpty())
    {
        presetBox.addSectionHeading ("User");

        for (const auto& name : userPresets)
        {
            presetNames.add (name);
            presetBox.addItem (name, itemId++);
        }
    }

    presetBox.setText (processorRef.getPresetManager().getCurrentPresetName(),
                       juce::dontSendNotification);
}

void DarkGrainAudioProcessorEditor::applyPresetFromBox()
{
    const auto selected = presetBox.getSelectedId();

    if (selected <= 0)
        return;

    const auto index = selected - 1;

    if (! juce::isPositiveAndBelow (index, presetNames.size()))
        return;

    const auto name = presetNames[index];

    if (processorRef.getPresetManager().applyPreset (name))
        setStatus ("Loaded " + name);
    else
        setStatus ("Preset not found: " + name);
}

void DarkGrainAudioProcessorEditor::savePresetFromBox()
{
    auto name = presetBox.getText().trim();

    if (name.isEmpty() || processorRef.getPresetManager().isFactoryPreset (name))
        name = "User Preset "
             + juce::String (processorRef.getPresetManager().getUserPresetNames().size() + 1);

    if (processorRef.getPresetManager().saveUserPreset (name))
    {
        rebuildPresetList();
        presetBox.setText (name, juce::dontSendNotification);
        setStatus ("Saved preset: " + name);
    }
    else
    {
        setStatus ("Could not save preset");
    }
}

void DarkGrainAudioProcessorEditor::setStatus (const juce::String& message)
{
    statusLabel.setText (message, juce::dontSendNotification);
}
