#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "parameters/ParameterIDs.h"
#include "parameters/ParameterLayout.h"

#include <memory>

namespace
{
constexpr double gainSmoothingSeconds = 0.02;
constexpr double mixSmoothingSeconds = 0.02;
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
    using namespace darkgrain::parameters;

    inputGainValue = apvts.getRawParameterValue (ids::inputGain);
    outputGainValue = apvts.getRawParameterValue (ids::outputGain);
    bypassValue = apvts.getRawParameterValue (ids::bypass);

    reverbSizeValue = apvts.getRawParameterValue (ids::reverbSize);
    reverbDampingValue = apvts.getRawParameterValue (ids::reverbDamping);
    diffusionValue = apvts.getRawParameterValue (ids::diffusion);
    modulationRateValue = apvts.getRawParameterValue (ids::modulationRate);
    modulationDepthValue = apvts.getRawParameterValue (ids::modulationDepth);
    preDelayValue = apvts.getRawParameterValue (ids::preDelay);
    reverbMixValue = apvts.getRawParameterValue (ids::reverbMix);
    widthValue = apvts.getRawParameterValue (ids::width);
    dryWetValue = apvts.getRawParameterValue (ids::dryWet);
    freezeValue = apvts.getRawParameterValue (ids::freeze);

    jassert (inputGainValue != nullptr);
    jassert (outputGainValue != nullptr);
    jassert (bypassValue != nullptr);
    jassert (reverbSizeValue != nullptr);
    jassert (reverbDampingValue != nullptr);
    jassert (diffusionValue != nullptr);
    jassert (modulationRateValue != nullptr);
    jassert (modulationDepthValue != nullptr);
    jassert (preDelayValue != nullptr);
    jassert (reverbMixValue != nullptr);
    jassert (widthValue != nullptr);
    jassert (dryWetValue != nullptr);
    jassert (freezeValue != nullptr);
}

void DarkGrainAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    if (sampleRate <= 0.0)
        return;

    const auto blockSize = juce::jmax (1, samplesPerBlock);

    inputGainSmoothed.reset (sampleRate, gainSmoothingSeconds);
    outputGainSmoothed.reset (sampleRate, gainSmoothingSeconds);
    bypassMixSmoothed.reset (sampleRate, bypassSmoothingSeconds);
    reverbMixSmoothed.reset (sampleRate, mixSmoothingSeconds);
    dryWetSmoothed.reset (sampleRate, mixSmoothingSeconds);

    inputGainSmoothed.setCurrentAndTargetValue (decibelsToGain (inputGainValue->load()));
    outputGainSmoothed.setCurrentAndTargetValue (decibelsToGain (outputGainValue->load()));
    bypassMixSmoothed.setCurrentAndTargetValue (bypassValue->load() >= 0.5f ? 0.0f : 1.0f);
    reverbMixSmoothed.setCurrentAndTargetValue (reverbMixValue->load() * 0.01f);
    dryWetSmoothed.setCurrentAndTargetValue (dryWetValue->load() * 0.01f);

    // Pre-allocate every buffer the audio callback needs.
    dryBuffer.setSize (2, blockSize, false, true, true);
    wetBuffer.setSize (2, blockSize, false, true, true);
    dryBuffer.clear();
    wetBuffer.clear();

    reverb.prepare (sampleRate, blockSize);

    darkgrain::dsp::DarkReverb::Parameters reverbParameters;
    updateReverbParameters (reverbParameters);
    reverb.setParameters (reverbParameters);
    reverb.reset();

    inputMeterLevel.store (0.0f, std::memory_order_relaxed);
    outputMeterLevel.store (0.0f, std::memory_order_relaxed);
}

void DarkGrainAudioProcessor::releaseResources()
{
}

void DarkGrainAudioProcessor::updateReverbParameters (
    darkgrain::dsp::DarkReverb::Parameters& destination) const noexcept
{
    destination.sizeSeconds = reverbSizeValue->load (std::memory_order_relaxed);
    destination.damping = reverbDampingValue->load (std::memory_order_relaxed) * 0.01f;
    destination.diffusion = diffusionValue->load (std::memory_order_relaxed) * 0.01f;
    destination.modulationRateHz = modulationRateValue->load (std::memory_order_relaxed);
    destination.modulationDepth = modulationDepthValue->load (std::memory_order_relaxed) * 0.01f;
    destination.preDelayMs = preDelayValue->load (std::memory_order_relaxed);
    destination.width = widthValue->load (std::memory_order_relaxed) * 0.01f;
    destination.freeze = freezeValue->load (std::memory_order_relaxed) >= 0.5f;
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
    reverbMixSmoothed.setTargetValue (reverbMixValue->load (std::memory_order_relaxed) * 0.01f);
    dryWetSmoothed.setTargetValue (dryWetValue->load (std::memory_order_relaxed) * 0.01f);

    darkgrain::dsp::DarkReverb::Parameters reverbParameters;
    updateReverbParameters (reverbParameters);
    reverb.setParameters (reverbParameters);

    const auto capacity = juce::jmin (dryBuffer.getNumSamples(), wetBuffer.getNumSamples());

    if (capacity <= 0)
        return;

    int blockOffset = 0;

    while (blockOffset < numSamples)
    {
        const auto chunkSize = juce::jmin (capacity, numSamples - blockOffset);

        // Keep an untouched copy for the click-free bypass crossfade.
        for (int channel = 0; channel < numChannels; ++channel)
            dryBuffer.copyFrom (channel, 0, buffer, channel, blockOffset, chunkSize);

        // Input trim happens before the effect chain.
        for (int sample = 0; sample < chunkSize; ++sample)
        {
            const auto gain = inputGainSmoothed.getNextValue();

            for (int channel = 0; channel < numChannels; ++channel)
                buffer.setSample (channel, blockOffset + sample,
                                  buffer.getSample (channel, blockOffset + sample) * gain);
        }

        // Non-owning view of the trimmed input for this chunk.
        juce::AudioBuffer<float> inputView (buffer.getArrayOfWritePointers(),
                                            numChannels,
                                            blockOffset,
                                            chunkSize);

        reverb.process (inputView, wetBuffer, chunkSize);

        for (int sample = 0; sample < chunkSize; ++sample)
        {
            const auto reverbMix = reverbMixSmoothed.getNextValue();
            const auto dryWet = dryWetSmoothed.getNextValue();
            const auto outputGain = outputGainSmoothed.getNextValue();
            const auto bypassMix = bypassMixSmoothed.getNextValue();

            for (int channel = 0; channel < numChannels; ++channel)
            {
                const auto dry = buffer.getSample (channel, blockOffset + sample);
                const auto wet = wetBuffer.getSample (channel, sample);

                // Reverb returns 100% wet; reverbMix places it in the wet chain,
                // then dryWet blends the whole chain back against the clean dry.
                const auto wetChain = dry + (wet - dry) * reverbMix;
                auto processed = dry + (wetChain - dry) * dryWet;
                processed *= outputGain;

                const auto clean = dryBuffer.getSample (channel, sample);

                buffer.setSample (channel, blockOffset + sample,
                                  clean + (processed - clean) * bypassMix);
            }
        }

        blockOffset += chunkSize;
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
