#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "dsp/Scale.h"
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
    highPassValue = apvts.getRawParameterValue (ids::highPass);
    outputGainValue = apvts.getRawParameterValue (ids::outputGain);
    bypassValue = apvts.getRawParameterValue (ids::bypass);

    grainSizeValue = apvts.getRawParameterValue (ids::grainSize);
    grainDensityValue = apvts.getRawParameterValue (ids::grainDensity);
    grainSpreadValue = apvts.getRawParameterValue (ids::grainSpread);
    grainPitchValue = apvts.getRawParameterValue (ids::grainPitch);
    pitchScatterValue = apvts.getRawParameterValue (ids::pitchScatter);
    reverseAmountValue = apvts.getRawParameterValue (ids::reverseAmount);
    stereoSpreadValue = apvts.getRawParameterValue (ids::stereoSpread);
    freezeValue = apvts.getRawParameterValue (ids::freeze);

    particleDensityValue = apvts.getRawParameterValue (ids::particleDensity);
    particleBrightnessValue = apvts.getRawParameterValue (ids::particleBrightness);
    particleRandomnessValue = apvts.getRawParameterValue (ids::particleRandomness);
    particleSizeValue = apvts.getRawParameterValue (ids::particleSize);
    particlePitchValue = apvts.getRawParameterValue (ids::particlePitch);
    particleWidthValue = apvts.getRawParameterValue (ids::particleWidth);
    particleFeedbackValue = apvts.getRawParameterValue (ids::particleFeedback);
    particleMixValue = apvts.getRawParameterValue (ids::particleMix);

    reverbSizeValue = apvts.getRawParameterValue (ids::reverbSize);
    reverbDampingValue = apvts.getRawParameterValue (ids::reverbDamping);
    diffusionValue = apvts.getRawParameterValue (ids::diffusion);
    modulationRateValue = apvts.getRawParameterValue (ids::modulationRate);
    modulationDepthValue = apvts.getRawParameterValue (ids::modulationDepth);
    preDelayValue = apvts.getRawParameterValue (ids::preDelay);
    reverbMixValue = apvts.getRawParameterValue (ids::reverbMix);

    toneDarkValue = apvts.getRawParameterValue (ids::toneDark);
    saturationValue = apvts.getRawParameterValue (ids::saturation);
    widthValue = apvts.getRawParameterValue (ids::width);
    dryWetValue = apvts.getRawParameterValue (ids::dryWet);
    scaleValue = apvts.getRawParameterValue (ids::scale);

    jassert (inputGainValue != nullptr && highPassValue != nullptr);
    jassert (outputGainValue != nullptr && bypassValue != nullptr);
    jassert (grainSizeValue != nullptr && grainDensityValue != nullptr);
    jassert (grainSpreadValue != nullptr && grainPitchValue != nullptr);
    jassert (pitchScatterValue != nullptr && reverseAmountValue != nullptr);
    jassert (stereoSpreadValue != nullptr && freezeValue != nullptr);
    jassert (particleDensityValue != nullptr && particleBrightnessValue != nullptr);
    jassert (particleSizeValue != nullptr && particlePitchValue != nullptr);
    jassert (particleWidthValue != nullptr && particleFeedbackValue != nullptr);
    jassert (particleMixValue != nullptr && particleRandomnessValue != nullptr);
    jassert (reverbSizeValue != nullptr && reverbDampingValue != nullptr);
    jassert (diffusionValue != nullptr && modulationRateValue != nullptr);
    jassert (modulationDepthValue != nullptr && preDelayValue != nullptr);
    jassert (reverbMixValue != nullptr && toneDarkValue != nullptr);
    jassert (saturationValue != nullptr && widthValue != nullptr);
    jassert (dryWetValue != nullptr);
    jassert (scaleValue != nullptr);

    presetManager = std::make_unique<darkgrain::state::PresetManager> (apvts);
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

    highPassLeft.prepare (sampleRate);
    highPassRight.prepare (sampleRate);
    toneLeft.prepare (sampleRate);
    toneRight.prepare (sampleRate);
    saturation.prepare (sampleRate);
    stereoWidth.prepare (sampleRate);
    limiter.prepare (sampleRate);

    grainEngine.prepare (sampleRate);
    particleEngine.prepare (sampleRate);
    particleEngine.setSource (&grainEngine.getBuffer());

    reverbEngine.prepare (sampleRate, blockSize);

    const auto prepareBuffer = [blockSize] (juce::AudioBuffer<float>& buffer)
    {
        buffer.setSize (2, blockSize, false, true, true);
        buffer.clear();
    };

    // Pre-allocate every buffer the audio callback needs.
    prepareBuffer (dryBuffer);
    prepareBuffer (dryGainedBuffer);
    prepareBuffer (grainBuffer);
    prepareBuffer (particleBuffer);
    prepareBuffer (wetChainBuffer);
    prepareBuffer (reverbBuffer);

    updateParameterTargets();

    // Start from the current targets so the first block does not glide in.
    inputGainSmoothed.setCurrentAndTargetValue (inputGainSmoothed.getTargetValue());
    outputGainSmoothed.setCurrentAndTargetValue (outputGainSmoothed.getTargetValue());
    bypassMixSmoothed.setCurrentAndTargetValue (bypassMixSmoothed.getTargetValue());
    reverbMixSmoothed.setCurrentAndTargetValue (reverbMixSmoothed.getTargetValue());
    dryWetSmoothed.setCurrentAndTargetValue (dryWetSmoothed.getTargetValue());

    reverbEngine.reset();

    inputMeterLevel.store (0.0f, std::memory_order_relaxed);
    outputMeterLevel.store (0.0f, std::memory_order_relaxed);
}

void DarkGrainAudioProcessor::releaseResources()
{
}

void DarkGrainAudioProcessor::updateParameterTargets() noexcept
{
    inputGainSmoothed.setTargetValue (decibelsToGain (inputGainValue->load (std::memory_order_relaxed)));
    outputGainSmoothed.setTargetValue (decibelsToGain (outputGainValue->load (std::memory_order_relaxed)));
    bypassMixSmoothed.setTargetValue (bypassValue->load (std::memory_order_relaxed) >= 0.5f ? 0.0f : 1.0f);
    reverbMixSmoothed.setTargetValue (reverbMixValue->load (std::memory_order_relaxed) * 0.01f);
    dryWetSmoothed.setTargetValue (dryWetValue->load (std::memory_order_relaxed) * 0.01f);

    const auto highPass = highPassValue->load (std::memory_order_relaxed);
    highPassLeft.setCutoff (highPass);
    highPassRight.setCutoff (highPass);

    const auto darkness = toneDarkValue->load (std::memory_order_relaxed) * 0.01f;
    toneLeft.setDarkness (darkness);
    toneRight.setDarkness (darkness);

    saturation.setAmount (saturationValue->load (std::memory_order_relaxed) * 0.01f);
    stereoWidth.setWidth (widthValue->load (std::memory_order_relaxed) * 0.01f);

    darkgrain::dsp::GrainParameters grainParameters;
    grainParameters.sizeMs = grainSizeValue->load (std::memory_order_relaxed);
    grainParameters.density = grainDensityValue->load (std::memory_order_relaxed);
    grainParameters.spread = grainSpreadValue->load (std::memory_order_relaxed) * 0.01f;
    grainParameters.pitchSemitones = grainPitchValue->load (std::memory_order_relaxed);
    grainParameters.pitchScatterSemitones = pitchScatterValue->load (std::memory_order_relaxed);
    grainParameters.reverseAmount = reverseAmountValue->load (std::memory_order_relaxed) * 0.01f;
    grainParameters.stereoSpread = stereoSpreadValue->load (std::memory_order_relaxed) * 0.01f;
    grainParameters.freeze = freezeValue->load (std::memory_order_relaxed) >= 0.5f;

    const auto scaleIndex = static_cast<int> (scaleValue->load (std::memory_order_relaxed));
    const auto scaleMask = darkgrain::dsp::scales::maskForIndex (scaleIndex);
    grainParameters.scaleMask = scaleMask;
    grainEngine.setParameters (grainParameters);

    darkgrain::dsp::ParticleParameters particleParameters;
    particleParameters.density = particleDensityValue->load (std::memory_order_relaxed) * 0.01f;
    particleParameters.sizeMs = particleSizeValue->load (std::memory_order_relaxed);
    particleParameters.brightness = particleBrightnessValue->load (std::memory_order_relaxed) * 0.01f;
    particleParameters.pitchSemitones = particlePitchValue->load (std::memory_order_relaxed);
    particleParameters.panWidth = particleWidthValue->load (std::memory_order_relaxed) * 0.01f;
    particleParameters.randomness = particleRandomnessValue->load (std::memory_order_relaxed) * 0.01f;
    particleParameters.feedback = particleFeedbackValue->load (std::memory_order_relaxed) * 0.01f;
    particleParameters.mix = particleMixValue->load (std::memory_order_relaxed) * 0.01f;
    particleParameters.scaleMask = scaleMask;
    particleEngine.setParameters (particleParameters);

    darkgrain::dsp::DarkReverb::Parameters reverbParameters;
    updateReverbParameters (reverbParameters);
    reverbEngine.setParameters (reverbParameters);
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

    updateParameterTargets();

    auto capacity = juce::jmin (dryBuffer.getNumSamples(), dryGainedBuffer.getNumSamples());
    capacity = juce::jmin (capacity, grainBuffer.getNumSamples());
    capacity = juce::jmin (capacity, particleBuffer.getNumSamples());
    capacity = juce::jmin (capacity, wetChainBuffer.getNumSamples());
    capacity = juce::jmin (capacity, reverbBuffer.getNumSamples());

    if (capacity <= 0)
        return;

    int blockOffset = 0;

    while (blockOffset < numSamples)
    {
        const auto chunkSize = juce::jmin (capacity, numSamples - blockOffset);
        processChunk (buffer, blockOffset, chunkSize, numChannels);
        blockOffset += chunkSize;
    }

    float outputPeak = 0.0f;

    for (int channel = 0; channel < numChannels; ++channel)
        outputPeak = juce::jmax (outputPeak, buffer.getMagnitude (channel, 0, numSamples));

    outputMeterLevel.store (outputPeak, std::memory_order_relaxed);
}

void DarkGrainAudioProcessor::processChunk (juce::AudioBuffer<float>& buffer,
                                            int blockOffset,
                                            int chunkSize,
                                            int numChannels) noexcept
{
    juce::AudioBuffer<float> inputView (buffer.getArrayOfWritePointers(),
                                        numChannels,
                                        blockOffset,
                                        chunkSize);

    // 1. Keep an untouched copy for the click-free bypass crossfade.
    for (int channel = 0; channel < numChannels; ++channel)
        dryBuffer.copyFrom (channel, 0, inputView, channel, 0, chunkSize);

    // 2. Input trim happens before the wet chain.
    for (int sample = 0; sample < chunkSize; ++sample)
    {
        const auto gain = inputGainSmoothed.getNextValue();

        for (int channel = 0; channel < numChannels; ++channel)
            inputView.setSample (channel, sample, inputView.getSample (channel, sample) * gain);
    }

    // 3. Keep the trimmed dry signal for the final wet/dry blend.
    for (int channel = 0; channel < numChannels; ++channel)
        dryGainedBuffer.copyFrom (channel, 0, inputView, channel, 0, chunkSize);

    // 4. High-pass the material that feeds the texture engines.
    for (int sample = 0; sample < chunkSize; ++sample)
    {
        inputView.setSample (0, sample, highPassLeft.process (inputView.getSample (0, sample)));

        if (numChannels > 1)
            inputView.setSample (1, sample, highPassRight.process (inputView.getSample (1, sample)));
    }

    // 5. Grains and particles read from the same circular history.
    grainEngine.process (inputView, grainBuffer, chunkSize);
    particleEngine.process (particleBuffer, chunkSize);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        wetChainBuffer.copyFrom (channel, 0, grainBuffer, channel, 0, chunkSize);
        wetChainBuffer.addFrom (channel, 0, particleBuffer, channel, 0, chunkSize);
    }

    if (numChannels == 1)
        wetChainBuffer.copyFrom (1, 0, wetChainBuffer, 0, 0, chunkSize);

    // 6. Dark FDN reverb on the wet chain.
    reverbEngine.process (wetChainBuffer, reverbBuffer, chunkSize);

    // 7. Reverb blend, tone, saturation, width, limiter and final mix.
    for (int sample = 0; sample < chunkSize; ++sample)
    {
        const auto reverbMix = reverbMixSmoothed.getNextValue();
        const auto dryWet = dryWetSmoothed.getNextValue();
        const auto outputGain = outputGainSmoothed.getNextValue();
        const auto bypassMix = bypassMixSmoothed.getNextValue();

        auto left = wetChainBuffer.getSample (0, sample);
        auto right = numChannels > 1 ? wetChainBuffer.getSample (1, sample) : left;

        const auto reverbLeft = reverbBuffer.getSample (0, sample);
        const auto reverbRight = numChannels > 1 ? reverbBuffer.getSample (1, sample) : reverbLeft;

        left += (reverbLeft - left) * reverbMix;
        right += (reverbRight - right) * reverbMix;

        left = toneLeft.process (left);
        right = toneRight.process (right);

        left = saturation.process (left);
        right = saturation.process (right);

        stereoWidth.process (left, right);

        const auto dryLeft = dryGainedBuffer.getSample (0, sample);
        const auto dryRight = numChannels > 1 ? dryGainedBuffer.getSample (1, sample) : dryLeft;

        auto outLeft = dryLeft + (left - dryLeft) * dryWet;
        auto outRight = dryRight + (right - dryRight) * dryWet;

        outLeft *= outputGain;
        outRight *= outputGain;

        // The safety clipper is the last stage before the host bypass blend, so
        // output gain can never push the processed signal past full scale.
        outLeft = limiter.process (outLeft);
        outRight = limiter.process (outRight);

        const auto cleanLeft = dryBuffer.getSample (0, sample);
        const auto cleanRight = numChannels > 1 ? dryBuffer.getSample (1, sample) : cleanLeft;

        inputView.setSample (0, sample, cleanLeft + (outLeft - cleanLeft) * bypassMix);

        if (numChannels > 1)
            inputView.setSample (1, sample, cleanRight + (outRight - cleanRight) * bypassMix);
    }
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
