#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

/**
    DarkGrain's audio processor.

    Milestone 1 is deliberately a clean stereo passthrough. It establishes the
    APVTS parameter tree, state persistence, smoothed input/output gain, and a
    click-free host bypass crossfade. DSP engines are added in later milestones.
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
    double getTailLengthSeconds() const override { return 0.0; }

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

private:
    std::atomic<float>* inputGainValue = nullptr;
    std::atomic<float>* outputGainValue = nullptr;
    std::atomic<float>* bypassValue = nullptr;

    juce::SmoothedValue<float> inputGainSmoothed;
    juce::SmoothedValue<float> outputGainSmoothed;
    juce::SmoothedValue<float> bypassMixSmoothed;

    std::atomic<float> inputMeterLevel { 0.0f };
    std::atomic<float> outputMeterLevel { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DarkGrainAudioProcessor)
};
