#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/DarkReverb.h"

#include <atomic>

/**
    DarkGrain's audio processor.

    Milestone 1 established the APVTS parameter tree, state persistence, smoothed
    input/output gain, and a click-free host bypass crossfade. Milestone 2 adds the
    dark feedback-delay-network reverb, including pre-delay, damping, diffusion,
    modulation, stereo width and wet/dry blending.

    The grain and particle engines arrive in later milestones.
*/
class DarkGrainAudioProcessor final : public juce::AudioProcessor
{
public:
    DarkGrainAudioProcessor();
    ~DarkGrainAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Init"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    juce::AudioProcessorValueTreeState apvts;

    /** Current input peak in linear amplitude (pre-gain, pre-processing). */
    float getInputMeterLevel() const noexcept
    {
        return inputMeterLevel.load (std::memory_order_relaxed);
    }

    /** Current output peak in linear amplitude. */
    float getOutputMeterLevel() const noexcept
    {
        return outputMeterLevel.load (std::memory_order_relaxed);
    }

    /** True while the reverb is in freeze mode. Read by the editor. */
    bool isFrozen() const noexcept
    {
        return freezeValue != nullptr && freezeValue->load (std::memory_order_relaxed) >= 0.5f;
    }

private:
    void updateReverbParameters (darkgrain::dsp::DarkReverb::Parameters& destination) const noexcept;

    std::atomic<float>* inputGainValue = nullptr;
    std::atomic<float>* outputGainValue = nullptr;
    std::atomic<float>* bypassValue = nullptr;

    std::atomic<float>* reverbSizeValue = nullptr;
    std::atomic<float>* reverbDampingValue = nullptr;
    std::atomic<float>* diffusionValue = nullptr;
    std::atomic<float>* modulationRateValue = nullptr;
    std::atomic<float>* modulationDepthValue = nullptr;
    std::atomic<float>* preDelayValue = nullptr;
    std::atomic<float>* reverbMixValue = nullptr;
    std::atomic<float>* widthValue = nullptr;
    std::atomic<float>* dryWetValue = nullptr;
    std::atomic<float>* freezeValue = nullptr;

    juce::SmoothedValue<float> inputGainSmoothed;
    juce::SmoothedValue<float> outputGainSmoothed;
    juce::SmoothedValue<float> bypassMixSmoothed;
    juce::SmoothedValue<float> reverbMixSmoothed;
    juce::SmoothedValue<float> dryWetSmoothed;

    darkgrain::dsp::DarkReverb reverb;

    // Pre-allocated in prepareToPlay(); processBlock() never allocates.
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> wetBuffer;

    std::atomic<float> inputMeterLevel { 0.0f };
    std::atomic<float> outputMeterLevel { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DarkGrainAudioProcessor)
};
