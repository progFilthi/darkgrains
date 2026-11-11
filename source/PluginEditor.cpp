#include "PluginEditor.h"

#include "PluginProcessor.h"
#include "parameters/ParameterIDs.h"

#include <array>
#include <cmath>

namespace
{
const auto backgroundColour = juce::Colour::fromRGB (9, 9, 12);
const auto backgroundWarmColour = juce::Colour::fromRGB (26, 14, 10);
const auto panelColour = juce::Colour::fromRGB (22, 20, 21);
const auto knobColour = juce::Colour::fromRGB (34, 30, 29);
const auto dividerColour = juce::Colour::fromRGB (52, 43, 38);
const auto trackColour = juce::Colour::fromRGB (58, 49, 44);
const auto accentColour = juce::Colour::fromRGB (255, 132, 40);
const auto accentBrightColour = juce::Colour::fromRGB (255, 184, 112);
const auto textPrimaryColour = juce::Colour::fromRGB (241, 234, 227);
const auto textDimColour = juce::Colour::fromRGB (151, 142, 135);

juce::Font makeFont (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

float normaliseParameter (float value, float minimum, float maximum) noexcept
{
    if (maximum <= minimum)
        return 0.0f;

    return juce::jlimit (0.0f, 1.0f, (value - minimum) / (maximum - minimum));
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
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accentColour.withAlpha (0.28f));
        setColour (juce::PopupMenu::highlightedTextColourId, textPrimaryColour);
        setColour (juce::TextButton::buttonColourId, panelColour);
        setColour (juce::TextButton::buttonOnColourId, accentColour.withAlpha (0.35f));
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
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (5.0f);
        const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centreX = bounds.getCentreX();
        const auto centreY = bounds.getCentreY();
        const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const auto lineWidth = juce::jmax (2.0f, radius * 0.13f);
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

        const auto knobRadius = radius * 0.62f;

        // Soft outer glow so the knobs feel lit rather than flat.
        g.setGradientFill (juce::ColourGradient (accentColour.withAlpha (0.10f + 0.16f * sliderPos),
                                                 centreX, centreY - knobRadius,
                                                 juce::Colours::transparentBlack,
                                                 centreX, centreY + knobRadius * 2.4f,
                                                 true));
        g.fillEllipse (juce::Rectangle<float> (knobRadius * 3.0f, knobRadius * 3.0f)
                           .withCentre ({ centreX, centreY }));

        g.setColour (knobColour);
        g.fillEllipse (juce::Rectangle<float> (knobRadius * 2.0f, knobRadius * 2.0f)
                           .withCentre ({ centreX, centreY }));

        g.setColour (dividerColour);
        g.drawEllipse (juce::Rectangle<float> (knobRadius * 2.0f, knobRadius * 2.0f)
                           .withCentre ({ centreX, centreY }),
                       1.0f);

        const auto pointerWidth = juce::jmax (2.0f, radius * 0.10f);
        juce::Path pointer;
        pointer.addRoundedRectangle (-pointerWidth * 0.5f,
                                     -radius * 0.66f,
                                     pointerWidth,
                                     radius * 0.32f,
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
        g.setFont (makeFont (9.5f));
        g.setColour (textDimColour);
        g.drawText (label, row.removeFromLeft (24), juce::Justification::centredLeft, false);

        const auto track = row.reduced (0, 3).toFloat();

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

class DarkGrainAudioProcessorEditor::AnimatedBackground final : public juce::Component,
                                                                private juce::Timer
{
public:
    explicit AnimatedBackground (DarkGrainAudioProcessor& processorToUse)
        : processor (processorToUse)
    {
        setInterceptsMouseClicks (false, false);
        setOpaque (true);

        for (auto& particle : particles)
            resetParticle (particle, true);

        startTimerHz (30);
    }

    ~AnimatedBackground() override
    {
        stopTimer();
    }

    void setFocusPoint (juce::Point<float> point)
    {
        focus = point;
        repaint();
    }

    void setFrozen (bool shouldBeFrozen)
    {
        frozen = shouldBeFrozen;
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        g.setGradientFill (juce::ColourGradient (backgroundColour, 0.0f, 0.0f,
                                                 backgroundWarmColour, 0.0f, bounds.getHeight(),
                                                 false));
        g.fillRect (bounds);

        // Faint vertical grid, like a dark studio wall.
        g.setColour (juce::Colours::white.withAlpha (0.018f));

        for (float x = 24.0f; x < bounds.getWidth(); x += 56.0f)
            g.drawVerticalLine (static_cast<int> (x), 0.0f, bounds.getHeight());

        const auto glow = frozen ? juce::Colour::fromRGB (131, 197, 255) : accentColour;
        const auto pulseRadius = 118.0f + 20.0f * std::sin (time * 0.9f) + pulse * 42.0f;

        const auto outerRadius = pulseRadius * 1.9f;
        g.setGradientFill (juce::ColourGradient (glow.withAlpha (0.13f + 0.20f * pulse),
                                                 focus.x, focus.y,
                                                 juce::Colours::transparentBlack,
                                                 focus.x + outerRadius, focus.y,
                                                 true));
        g.fillEllipse (juce::Rectangle<float> (outerRadius * 2.0f, outerRadius * 2.0f)
                           .withCentre (focus));

        for (int ring = 0; ring < 3; ++ring)
        {
            const auto ringRadius = 92.0f + ring * 30.0f
                                  + 4.0f * std::sin (time * (0.65f + ring * 0.18f)
                                                     + static_cast<float> (ring));
            g.setColour (glow.withAlpha (0.20f - static_cast<float> (ring) * 0.05f));
            g.drawEllipse (juce::Rectangle<float> (ringRadius * 2.0f, ringRadius * 2.0f)
                               .withCentre (focus),
                           1.0f);
        }

        // Rotating grain streaks around the core.
        const auto numStreaks = 22;

        for (int streak = 0; streak < numStreaks; ++streak)
        {
            const auto offset = static_cast<float> (streak) / static_cast<float> (numStreaks);
            const auto angle = time * 0.16f + offset * juce::MathConstants<float>::twoPi;
            const auto nearRadius = 74.0f + 16.0f * std::sin (time * 1.2f + offset * 9.0f);
            const auto farRadius = nearRadius + 20.0f + energy * 72.0f
                                 * (0.45f + 0.55f * std::sin (time * 1.6f + offset * 17.0f));
            const auto start = focus + juce::Point<float> (std::cos (angle), std::sin (angle)) * nearRadius;
            const auto end = focus + juce::Point<float> (std::cos (angle), std::sin (angle)) * farRadius;

            g.setColour (glow.withAlpha (0.05f + 0.22f * energy));
            g.drawLine ({ start, end }, 1.0f);
        }

        const auto activeParticles = 12 + static_cast<int> (energy * 140.0f);

        for (int index = 0; index < activeParticles; ++index)
        {
            const auto& particle = particles[static_cast<size_t> (index)];
            const auto x = particle.x * bounds.getWidth();
            const auto y = particle.y * bounds.getHeight();
            const auto size = particle.size * (0.55f + 1.15f * energy);
            const auto alpha = particle.alpha * (0.12f + 0.88f * energy);
            const auto particleColour = (frozen ? glow : accentBrightColour).withAlpha (alpha);

            g.setColour (particleColour.withMultipliedAlpha (0.22f));
            g.fillEllipse (juce::Rectangle<float> (size * 3.4f, size * 3.4f)
                               .withCentre ({ x, y }));
            g.setColour (particleColour);
            g.fillEllipse (juce::Rectangle<float> (size, size).withCentre ({ x, y }));
        }

        const auto coreRadius = 6.0f + 8.0f * pulse;
        g.setColour (glow.withAlpha (0.55f + 0.35f * pulse));
        g.fillEllipse (juce::Rectangle<float> (coreRadius * 2.0f, coreRadius * 2.0f)
                           .withCentre (focus));
    }

private:
    struct Particle
    {
        float x = 0.0f;
        float y = 0.0f;
        float speed = 0.2f;
        float drift = 1.0f;
        float phase = 0.0f;
        float size = 1.0f;
        float alpha = 0.5f;
    };

    void resetParticle (Particle& particle, bool anywhere) noexcept
    {
        particle.x = random.nextFloat();
        particle.y = anywhere ? random.nextFloat() : 1.02f + random.nextFloat() * 0.18f;
        particle.speed = 0.10f + random.nextFloat() * 0.45f;
        particle.drift = 0.4f + random.nextFloat() * 1.6f;
        particle.phase = random.nextFloat() * juce::MathConstants<float>::twoPi;
        particle.size = 0.8f + random.nextFloat() * 2.2f;
        particle.alpha = 0.25f + random.nextFloat() * 0.6f;
    }

    void timerCallback() override
    {
        const auto density = processor.apvts.getRawParameterValue (darkgrain::parameters::ids::grainDensity);
        const auto particleDensity = processor.apvts.getRawParameterValue (darkgrain::parameters::ids::particleDensity);
        const auto reverbMix = processor.apvts.getRawParameterValue (darkgrain::parameters::ids::reverbMix);
        const auto output = processor.getOutputMeterLevel();

        const auto targetEnergy = juce::jlimit (
            0.10f, 1.0f,
            0.10f
                + 0.62f * normaliseParameter (density != nullptr ? density->load() : 12.0f, 1.0f, 64.0f)
                + 0.24f * normaliseParameter (particleDensity != nullptr ? particleDensity->load() : 35.0f, 0.0f, 100.0f)
                + 0.10f * normaliseParameter (reverbMix != nullptr ? reverbMix->load() : 35.0f, 0.0f, 100.0f));

        energy += (targetEnergy - energy) * 0.07f;
        pulse = juce::jmax (pulse * 0.86f, juce::jlimit (0.0f, 1.0f, output));

        constexpr auto deltaSeconds = 1.0f / 30.0f;
        time += deltaSeconds;

        const auto motion = frozen ? 0.12f : (0.18f + energy * 1.25f);

        for (auto& particle : particles)
        {
            particle.y -= particle.speed * motion * deltaSeconds;
            particle.x += std::sin (time * particle.drift + particle.phase) * 0.0012f;

            if (particle.y < -0.05f)
                resetParticle (particle, false);

            if (particle.x < -0.02f)
                particle.x += 1.04f;
            else if (particle.x > 1.02f)
                particle.x -= 1.04f;
        }

        repaint();
    }

    DarkGrainAudioProcessor& processor;
    std::array<Particle, 160> particles;
    juce::Random random { 0x51ee7u };
    juce::Point<float> focus;
    float time = 0.0f;
    float energy = 0.25f;
    float pulse = 0.0f;
    bool frozen = false;
};

DarkGrainAudioProcessorEditor::DarkGrainAudioProcessorEditor (DarkGrainAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p)
{
    using namespace darkgrain::parameters;

    lookAndFeel = std::make_unique<DarkGrainLookAndFeel>();
    setLookAndFeel (lookAndFeel.get());

    animatedBackground = std::make_unique<AnimatedBackground> (processorRef);
    addAndMakeVisible (*animatedBackground);
    animatedBackground->toBack();

    addAndMakeVisible (titleLabel);
    titleLabel.setText ("DARKGRAIN", juce::dontSendNotification);
    titleLabel.setFont (makeFont (27.0f, true));
    titleLabel.setColour (juce::Label::textColourId, textPrimaryColour);
    titleLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (subtitleLabel);
    subtitleLabel.setText ("in-key granular texture", juce::dontSendNotification);
    subtitleLabel.setFont (makeFont (11.5f));
    subtitleLabel.setColour (juce::Label::textColourId, textDimColour);
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (statusLabel);
    statusLabel.setText ("Ready", juce::dontSendNotification);
    statusLabel.setFont (makeFont (10.0f));
    statusLabel.setColour (juce::Label::textColourId, textDimColour);
    statusLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (keyLabel);
    keyLabel.setText ("KEY", juce::dontSendNotification);
    keyLabel.setFont (makeFont (9.5f, true));
    keyLabel.setColour (juce::Label::textColourId, textDimColour);
    keyLabel.setJustificationType (juce::Justification::centredLeft);

    rootKeyBox.setJustificationType (juce::Justification::centredLeft);
    scaleBox.setJustificationType (juce::Justification::centredLeft);
    rootKeyBox.setTooltip ("Song root key");
    scaleBox.setTooltip ("Scale used to keep grains and particles in key");
    addAndMakeVisible (rootKeyBox);
    addAndMakeVisible (scaleBox);

    rootKeyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.apvts, ids::rootKey, rootKeyBox);
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.apvts, ids::scale, scaleBox);

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
        processorRef.apvts, ids::freeze, freezeButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.apvts, ids::bypass, bypassButton);

    addAndMakeVisible (advancedButton);
    advancedButton.setTooltip ("Show or hide the detailed controls");
    advancedButton.onClick = [this] { setAdvancedVisible (! advancedVisible); };

    freezeIndicatorLabel.setText ("FROZEN", juce::dontSendNotification);
    freezeIndicatorLabel.setFont (makeFont (10.0f, true));
    freezeIndicatorLabel.setColour (juce::Label::textColourId, accentBrightColour);
    freezeIndicatorLabel.setJustificationType (juce::Justification::centredRight);
    freezeIndicatorLabel.setVisible (false);
    addAndMakeVisible (freezeIndicatorLabel);

    grainMacroName.setText ("GRAINS", juce::dontSendNotification);
    grainMacroName.setFont (makeFont (13.0f, true));
    grainMacroName.setColour (juce::Label::textColourId, accentBrightColour);
    grainMacroName.setJustificationType (juce::Justification::centred);
    grainMacroName.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (grainMacroName);

    spaceMacroName.setText ("SPACE", juce::dontSendNotification);
    spaceMacroName.setFont (makeFont (13.0f, true));
    spaceMacroName.setColour (juce::Label::textColourId, accentBrightColour);
    spaceMacroName.setJustificationType (juce::Justification::centred);
    spaceMacroName.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (spaceMacroName);

    const auto configureMacro = [] (juce::Slider& slider)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 96, 20);
        slider.setRange (0.0, 100.0, 1.0);
        slider.setTextValueSuffix (" %");
        slider.setNumDecimalPlacesToDisplay (0);
        slider.setPopupDisplayEnabled (true, true, nullptr);
    };

    configureMacro (grainMacroSlider);
    configureMacro (spaceMacroSlider);
    grainMacroSlider.setTooltip ("Macro: grain density, size, particles and movement");
    spaceMacroSlider.setTooltip ("Macro: reverb size, mix, diffusion and width");
    grainMacroSlider.onValueChange = [this] { applyGrainMacro(); };
    spaceMacroSlider.onValueChange = [this] { applySpaceMacro(); };
    addAndMakeVisible (grainMacroSlider);
    addAndMakeVisible (spaceMacroSlider);

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
    syncMacroKnobsFromState();

    setOpaque (true);
    setResizable (false, false);
    setSize (980, 620);
    setAdvancedVisible (false);
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
    control->name.setFont (makeFont (10.5f));
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

        knob.name.setBounds (cell.removeFromTop (13));

        auto sliderArea = cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), 82),
                                                      juce::jmin (cell.getHeight(), 86));
        knob.slider.setBounds (sliderArea);
    }
}

void DarkGrainAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (18);

    headerBounds = area.removeFromTop (82);
    footerBounds = area.removeFromBottom (44);
    contentBounds = area;

    if (animatedBackground != nullptr)
    {
        animatedBackground->setBounds (getLocalBounds());
        animatedBackground->setFocusPoint (contentBounds.getCentre().toFloat());
    }

    auto header = headerBounds;
    auto titleArea = header.removeFromLeft (360);

    titleLabel.setBounds (titleArea.removeFromTop (30));
    subtitleLabel.setBounds (titleArea.removeFromTop (18));

    auto keyRow = titleArea.removeFromTop (26);
    keyLabel.setBounds (keyRow.removeFromLeft (30));
    rootKeyBox.setBounds (keyRow.removeFromLeft (58).reduced (0, 1));
    keyRow.removeFromLeft (6);
    scaleBox.setBounds (keyRow.removeFromLeft (150).reduced (0, 1));

    savePresetButton.setBounds (header.removeFromRight (66).withSizeKeepingCentre (66, 28));
    header.removeFromRight (8);
    presetBox.setBounds (header.removeFromRight (238).withSizeKeepingCentre (238, 28));

    meter->setBounds (footerBounds.removeFromRight (186).reduced (0, 8));

    auto footer = footerBounds;
    freezeButton.setBounds (footer.removeFromLeft (88).withSizeKeepingCentre (88, 26));
    footer.removeFromLeft (8);
    bypassButton.setBounds (footer.removeFromLeft (88).withSizeKeepingCentre (88, 26));
    footer.removeFromLeft (12);
    advancedButton.setBounds (footer.removeFromLeft (110).withSizeKeepingCentre (110, 26));
    footer.removeFromLeft (12);
    statusLabel.setBounds (footer);

    freezeIndicatorLabel.setBounds (contentBounds.getRight() - 104,
                                    contentBounds.getY() + 4,
                                    96,
                                    18);

    if (advancedVisible)
    {
        auto advancedArea = contentBounds;
        constexpr int gap = 12;

        auto row1 = advancedArea.removeFromTop ((advancedArea.getHeight() - gap) / 2);
        advancedArea.removeFromTop (gap);
        auto row2 = advancedArea;

        auto inputBounds = row1.removeFromLeft (200);
        row1.removeFromLeft (gap);

        const auto grainsWidth = (row1.getWidth() - gap) / 2;
        auto grainsBounds = row1.removeFromLeft (grainsWidth);
        row1.removeFromLeft (gap);
        auto particlesBounds = row1;

        const auto outputWidth = 376;
        auto spaceBounds = row2.removeFromLeft (row2.getWidth() - outputWidth - gap);
        row2.removeFromLeft (gap);
        auto outputBounds = row2;

        layoutPanel (*inputPanel, inputBounds);
        layoutPanel (*grainsPanel, grainsBounds);
        layoutPanel (*particlesPanel, particlesBounds);
        layoutPanel (*spacePanel, spaceBounds);
        layoutPanel (*outputPanel, outputBounds);
    }
    else
    {
        const auto centre = contentBounds.getCentre();
        constexpr int knobWidth = 198;
        constexpr int knobHeight = 220;
        constexpr int macroOffset = 246;

        const auto grainX = centre.x - macroOffset - knobWidth / 2;
        const auto spaceX = centre.x + macroOffset - knobWidth / 2;
        const auto nameY = centre.y - 132;

        grainMacroName.setBounds (grainX, nameY, knobWidth, 22);
        grainMacroSlider.setBounds (grainX, nameY + 20, knobWidth, knobHeight);
        spaceMacroName.setBounds (spaceX, nameY, knobWidth, 22);
        spaceMacroSlider.setBounds (spaceX, nameY + 20, knobWidth, knobHeight);
    }
}

void DarkGrainAudioProcessorEditor::paint (juce::Graphics& g)
{
    if (! advancedVisible)
        return;

    g.fillAll (backgroundColour);

    g.setColour (accentColour.withAlpha (0.65f));
    g.fillRect (0, 0, getWidth(), 2);

    for (const auto& panelPtr : panels)
    {
        const auto& panel = *panelPtr;

        g.setColour (panelColour);
        g.fillRoundedRectangle (panel.bounds.toFloat(), 8.0f);

        g.setColour (accentColour);
        g.setFont (makeFont (10.0f, true));
        g.drawText (panel.title,
                    panel.bounds.reduced (14).withHeight (16),
                    juce::Justification::centredLeft,
                    false);
    }

    g.setColour (dividerColour);
    g.fillRect (footerBounds.getX(), footerBounds.getY(), footerBounds.getWidth(), 1);

    g.setFont (makeFont (10.0f));
    g.setColour (textDimColour);
    g.drawText (juce::String ("0 samples latency  |  v" DARKGRAIN_VERSION),
                footerBounds.withTrimmedRight (190).reduced (0, 1),
                juce::Justification::centredRight,
                false);
}

void DarkGrainAudioProcessorEditor::timerCallback()
{
    const auto keyText = rootKeyBox.getText().trim() + "  |  "
                       + scaleBox.getText().trim().toLowerCase();

    if (keyText != displayedKeyText)
    {
        displayedKeyText = keyText;
        subtitleLabel.setText (keyText + "  |  in-key granular texture",
                               juce::dontSendNotification);
    }

    const auto frozen = processorRef.isFrozen();

    if (frozen != wasFrozen)
    {
        wasFrozen = frozen;
        freezeIndicatorLabel.setVisible (! advancedVisible && frozen);
        animatedBackground->setFrozen (frozen);
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
    {
        syncMacroKnobsFromState();
        setStatus ("Loaded " + name);
    }
    else
    {
        setStatus ("Preset not found: " + name);
    }
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

void DarkGrainAudioProcessorEditor::setAdvancedVisible (bool shouldShowAdvanced)
{
    advancedVisible = shouldShowAdvanced;

    if (animatedBackground != nullptr)
        animatedBackground->setVisible (! advancedVisible);

    for (auto& knob : knobs)
    {
        knob->name.setVisible (advancedVisible);
        knob->slider.setVisible (advancedVisible);
    }

    grainMacroName.setVisible (! advancedVisible);
    grainMacroSlider.setVisible (! advancedVisible);
    spaceMacroName.setVisible (! advancedVisible);
    spaceMacroSlider.setVisible (! advancedVisible);

    freezeIndicatorLabel.setVisible (! advancedVisible && wasFrozen);
    advancedButton.setButtonText (advancedVisible ? "PERFORM" : "ADVANCED");

    if (! advancedVisible)
        syncMacroKnobsFromState();

    resized();
    repaint();
}

void DarkGrainAudioProcessorEditor::syncMacroKnobsFromState()
{
    suppressMacroUpdates = true;

    const auto density = getParameterValue (darkgrain::parameters::ids::grainDensity);
    const auto densityPosition = std::sqrt (juce::jlimit (0.0f, 1.0f, (density - 1.0f) / 63.0f));

    grainMacroSlider.setValue (densityPosition * 100.0f, juce::dontSendNotification);
    spaceMacroSlider.setValue (getParameterValue (darkgrain::parameters::ids::reverbMix),
                               juce::dontSendNotification);

    suppressMacroUpdates = false;
}

void DarkGrainAudioProcessorEditor::applyGrainMacro()
{
    if (suppressMacroUpdates)
        return;

    using namespace darkgrain::parameters;

    const auto amount = static_cast<float> (grainMacroSlider.getValue()) * 0.01f;

    setParameterValue (ids::grainDensity, 1.0f + amount * amount * 63.0f);
    setParameterValue (ids::grainSize, 135.0f - 90.0f * amount);
    setParameterValue (ids::grainSpread, 15.0f + 80.0f * amount);
    setParameterValue (ids::stereoSpread, 30.0f + 55.0f * amount);
    setParameterValue (ids::reverseAmount, 12.0f + 32.0f * amount);
    setParameterValue (ids::particleDensity, 8.0f + 70.0f * amount);
    setParameterValue (ids::particleMix, 8.0f + 55.0f * amount);
    setParameterValue (ids::dryWet, 28.0f + 47.0f * amount);
}

void DarkGrainAudioProcessorEditor::applySpaceMacro()
{
    if (suppressMacroUpdates)
        return;

    using namespace darkgrain::parameters;

    const auto amount = static_cast<float> (spaceMacroSlider.getValue()) * 0.01f;

    setParameterValue (ids::reverbMix, 6.0f + 70.0f * amount);
    setParameterValue (ids::reverbSize, 0.6f + 8.6f * amount);
    setParameterValue (ids::diffusion, 40.0f + 55.0f * amount);
    setParameterValue (ids::reverbDamping, 70.0f - 30.0f * amount);
    setParameterValue (ids::preDelay, 10.0f + 120.0f * amount);
    setParameterValue (ids::width, 105.0f + 60.0f * amount);
}

void DarkGrainAudioProcessorEditor::setParameterValue (const char* id, float value)
{
    if (auto* parameter = processorRef.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

float DarkGrainAudioProcessorEditor::getParameterValue (const char* id) const
{
    if (const auto* parameter = processorRef.apvts.getParameter (id))
        return parameter->convertFrom0to1 (parameter->getValue());

    return 0.0f;
}
