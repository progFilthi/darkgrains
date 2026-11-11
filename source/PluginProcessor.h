#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/DarkReverb.h"
#include "dsp/Filter.h"
#include "dsp/GrainEngine.h"
#include "dsp/Limiter.h"
#include "dsp/ParticleEngine.h"
#include "dsp/Saturation.h"
#include "dsp/StereoWidth.h"
#include "state/PresetManager.h"

#include <atomic>
#include <memory>

/**
    DarkGrain's audio processor.

    Milestone 1 established the APVTS parameter tree, state persistence, smoothed
    input/output gain, and a click-free host bypass crossfade. Milestone 2 added the
    dark feedback-delay-network reverb. Milestones 3–5 added the granular engine,
    the particle generator, tone shaping, saturation, stereo width, the safety
    limiter, the full dark editor, and the preset system.

    Signal flow:

        input trim -> high-pass -> grain engine -> particles
        -> FDN reverb -> tone shaping -> saturation -> width
        -> dry/wet -> output gain -> safety limiter -> bypass crossfade

    The dry path stays clean and phase-coherent; the wet chain is built in
    parallel and blended back with the dry signal. All buffers and engines are
    prepared in prepareToPlay(), so processBlock() never allocates or locks.
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
    double getTailLengthSeconds() const override { return 12.0; }

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

    /** Preset handling runs on the message thread from the editor. */
    darkgrain::state::PresetManager& getPresetManager() noexcept { return *presetManager; }

    /** Current input peak in linear amplitude (pre-processing). */
    float getInputMeterLevel() const noexcept
    {
        return inputMeterLevel.load (std::memory_order_relaxed);
    }

    /** Current output peak in linear amplitude. */
    float getOutputMeterLevel() const noexcept
    {
        return outputMeterLevel.load (std::memory_order_relaxed);
    }

    /** True while the grain engine is holding its buffer. Read by the editor. */
    bool isFrozen() const noexcept
    {
        return freezeValue != nullptr && freezeValue->load (std::memory_order_relaxed) >= 0.5f;
    }

private:
    void updateParameterTargets() noexcept;
    void updateReverbParameters (darkgrain::dsp::DarkReverb::Parameters& destination) const noexcept;
    void processChunk (juce::AudioBuffer<float>& buffer,
                       int blockOffset,
                       int chunkSize,
                       int numChannels) noexcept;

    std::atomic<float>* inputGainValue = nullptr;
    std::atomic<float>* highPassValue = nullptr;
    std::atomic<float>* outputGainValue = nullptr;
    std::atomic<float>* bypassValue = nullptr;

    std::atomic<float>* grainSizeValue = nullptr;
    std::atomic<float>* grainDensityValue = nullptr;
    std::atomic<float>* grainSpreadValue = nullptr;
    std::atomic<float>* grainPitchValue = nullptr;
    std::atomic<float>* pitchScatterValue = nullptr;
    std::atomic<float>* reverseAmountValue = nullptr;
    std::atomic<float>* stereoSpreadValue = nullptr;
    std::atomic<float>* freezeValue = nullptr;

    std::atomic<float>* particleDensityValue = nullptr;
    std::atomic<float>* particleBrightnessValue = nullptr;
    std::atomic<float>* particleRandomnessValue = nullptr;
    std::atomic<float>* particleSizeValue = nullptr;
    std::atomic<float>* particlePitchValue = nullptr;
    std::atomic<float>* particleWidthValue = nullptr;
    std::atomic<float>* particleFeedbackValue = nullptr;
    std::atomic<float>* particleMixValue = nullptr;

    std::atomic<float>* reverbSizeValue = nullptr;
    std::atomic<float>* reverbDampingValue = nullptr;
    std::atomic<float>* diffusionValue = nullptr;
    std::atomic<float>* modulationRateValue = nullptr;
    std::atomic<float>* modulationDepthValue = nullptr;
    std::atomic<float>* preDelayValue = nullptr;
    std::atomic<float>* reverbMixValue = nullptr;

    std::atomic<float>* toneDarkValue = nullptr;
    std::atomic<float>* saturationValue = nullptr;
    std::atomic<float>* widthValue = nullptr;
    std::atomic<float>* dryWetValue = nullptr;

    juce::SmoothedValue<float> inputGainSmoothed;
    juce::SmoothedValue<float> outputGainSmoothed;
    juce::SmoothedValue<float> bypassMixSmoothed;
    juce::SmoothedValue<float> reverbMixSmoothed;
    juce::SmoothedValue<float> dryWetSmoothed;

    darkgrain::dsp::GrainEngine grainEngine;
    darkgrain::dsp::ParticleEngine particleEngine;
    darkgrain::dsp::DarkReverb reverbEngine;
    darkgrain::dsp::OnePoleHighPass highPassLeft;
    darkgrain::dsp::OnePoleHighPass highPassRight;
    darkgrain::dsp::TiltFilter toneLeft;
    darkgrain::dsp::TiltFilter toneRight;
    darkgrain::dsp::Saturation saturation;
    darkgrain::dsp::StereoWidth stereoWidth;
    darkgrain::dsp::Limiter limiter;

    std::unique_ptr<darkgrain::state::PresetManager> presetManager;

    // Pre-allocated in prepareToPlay(); processBlock() never allocates.
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> dryGainedBuffer;
    juce::AudioBuffer<float> grainBuffer;
    juce::AudioBuffer<float> particleBuffer;
    juce::AudioBuffer<float> wetChainBuffer;
    juce::AudioBuffer<float> reverbBuffer;

    std::atomic<float> inputMeterLevel { 0.0f };
    std::atomic<float> outputMeterLevel { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DarkGrainAudioProcessor)
};
