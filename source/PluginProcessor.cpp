#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "parameters/ParameterIDs.h"
#include "parameters/ParameterLayout.h"

#include <memory>

namespace
{
constexpr double gainSmoothingSeconds = 0.02;
constexpr double bypassSmoothingSeconds = 0.03;

float decibelsToGain (float decibels) noexcept
{
    return juce::Decibels::decibelsToGain (decibels, -60.0f);
}
} // namespace

DarkGrainAudioProcessor::DarkGrainAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", darkgrain::parameters::createParameterLayout())
{
    inputGainValue = apvts.getRawParameterValue (darkgrain::parameters::ids::inputGain);
    outputGainValue = apvts.getRawParameterValue (darkgrain::parameters::ids::outputGain);
    bypassValue = apvts.getRawParameterValue (darkgrain::parameters::ids::bypass);

    jassert (inputGainValue != nullptr);
    jassert (outputGainValue != nullptr);
    jassert (bypassValue != nullptr);
}

void DarkGrainAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    if (sampleRate <= 0.0)
        return;

    inputGainSmoothed.reset (sampleRate, gainSmoothingSeconds);
    outputGainSmoothed.reset (sampleRate, gainSmoothingSeconds);
    bypassMixSmoothed.reset (sampleRate, bypassSmoothingSeconds);

    inputGainSmoothed.setCurrentAndTargetValue (decibelsToGain (inputGainValue->load()));
    outputGainSmoothed.setCurrentAndTargetValue (decibelsToGain (outputGainValue->load()));
    bypassMixSmoothed.setCurrentAndTargetValue (bypassValue->load() >= 0.5f ? 0.0f : 1.0f);

    inputMeterLevel.store (0.0f, std::memory_order_relaxed);
    outputMeterLevel.store (0.0f, std::memory_order_relaxed);
}

void DarkGrainAudioProcessor::releaseResources()
{
}

void DarkGrainAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = juce::jmin (2, buffer.getNumChannels());

    if (numSamples <= 0 || numChannels <= 0)
        return;

    float inputPeak = 0.0f;

    for (int channel = 0; channel < numChannels; ++channel)
        inputPeak = juce::jmax (inputPeak, buffer.getMagnitude (channel, 0, numSamples));

    inputMeterLevel.store (inputPeak, std::memory_order_relaxed);

    inputGainSmoothed.setTargetValue (decibelsToGain (inputGainValue->load (std::memory_order_relaxed)));
    outputGainSmoothed.setTargetValue (decibelsToGain (outputGainValue->load (std::memory_order_relaxed)));
    bypassMixSmoothed.setTargetValue (bypassValue->load (std::memory_order_relaxed) >= 0.5f ? 0.0f : 1.0f);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto gain = inputGainSmoothed.getNextValue() * outputGainSmoothed.getNextValue();
        const auto bypassMix = bypassMixSmoothed.getNextValue();

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const auto dry = buffer.getSample (channel, sample);
            const auto wet = dry * gain;
            buffer.setSample (channel, sample, dry + (wet - dry) * bypassMix);
        }
    }

    float outputPeak = 0.0f;

    for (int channel = 0; channel < numChannels; ++channel)
        outputPeak = juce::jmax (outputPeak, buffer.getMagnitude (channel, 0, numSamples));

    outputMeterLevel.store (outputPeak, std::memory_order_relaxed);
}

bool DarkGrainAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

juce::AudioProcessorEditor* DarkGrainAudioProcessor::createEditor()
{
    return new DarkGrainAudioProcessorEditor (*this);
}

const juce::String DarkGrainAudioProcessor::getName() const
{
    return "DarkGrain";
}

void DarkGrainAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();

    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DarkGrainAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    const std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorParameter* DarkGrainAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter (darkgrain::parameters::ids::bypass);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DarkGrainAudioProcessor();
}
