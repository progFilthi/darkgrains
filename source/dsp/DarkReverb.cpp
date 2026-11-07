#include "dsp/DarkReverb.h"

#include <cmath>

namespace darkgrain::dsp
{
namespace
{
constexpr float twoPi = juce::MathConstants<float>::twoPi;

// Mutually prime delay lengths at 48 kHz (roughly 23-47 ms).
constexpr std::array<int, DarkReverb::numLines> baseDelayLengths {
    1123, 1291, 1453, 1601, 1777, 1931, 2089, 2237
};

// Short prime all-pass lengths for input diffusion at 48 kHz.
constexpr std::array<int, 4> allpassLengthsLeft { 149, 211, 263, 307 };
constexpr std::array<int, 4> allpassLengthsRight { 157, 223, 271, 313 };

constexpr float lowCutHz = 25.0f;
constexpr float maximumFeedbackGain = 0.9995f;

inline void hadamard8 (std::array<float, DarkReverb::numLines>& values) noexcept
{
    for (int length = 1; length < DarkReverb::numLines; length <<= 1)
    {
        for (int i = 0; i < DarkReverb::numLines; i += length << 1)
        {
            for (int j = i; j < i + length; ++j)
            {
                const auto a = values[static_cast<size_t> (j)];
                const auto b = values[static_cast<size_t> (j + length)];
                values[static_cast<size_t> (j)] = a + b;
                values[static_cast<size_t> (j + length)] = a - b;
            }
        }
    }

    constexpr float normalisation = 0.3535533905932738f; // 1 / sqrt(8)

    for (auto& value : values)
        value *= normalisation;
}

inline float softClip (float sample) noexcept
{
    constexpr float limit = 1.5f;
    const auto magnitude = std::abs (sample);

    if (magnitude >= limit)
        return sample > 0.0f ? 1.0f : -1.0f;

    return sample - (sample * sample * sample) / (3.0f * limit * limit);
}

inline int wrapIndex (int index, int length) noexcept
{
    if (length <= 0)
        return 0;

    index %= length;
    return index < 0 ? index + length : index;
}
} // namespace

float DarkReverb::Allpass::process (float input, float coefficient) noexcept
{
    const auto length = buffer.getNumSamples();

    if (length <= 0)
        return input;

    auto* data = buffer.getWritePointer (0);
    const auto delayed = data[writeIndex];
    const auto output = delayed - coefficient * input;

    data[writeIndex] = input + coefficient * output;
    writeIndex = wrapIndex (writeIndex + 1, length);

    return output;
}

void DarkReverb::Allpass::reset() noexcept
{
    buffer.clear();
    writeIndex = 0;
}

void DarkReverb::Line::reset() noexcept
{
    buffer.clear();
    writeIndex = 0;
    dampState = 0.0f;
    lowCutState = 0.0f;
    modPhase = 0.0f;
}

void DarkReverb::prepare (double newSampleRate, int maximumBlockSize)
{
    juce::ignoreUnused (maximumBlockSize);

    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;

    const auto modulationSamples = static_cast<int> (std::ceil (maximumModulationSeconds * sampleRate)) + 4;

    for (size_t i = 0; i < lines.size(); ++i)
    {
        auto& line = lines[i];

        line.baseDelaySamples = juce::jmax (
            16, static_cast<int> (std::lround (baseDelayLengths[i] * sampleRate / 48000.0)));
        line.buffer.setSize (1, line.baseDelaySamples + modulationSamples + 8, false, true, false);
        line.feedbackGain.reset (sampleRate, 0.08);
        line.reset();
    }

    for (size_t channel = 0; channel < diffusers.size(); ++channel)
    {
        for (size_t index = 0; index < diffusers[channel].size(); ++index)
        {
            const auto baseLength = channel == 0 ? allpassLengthsLeft[index]
                                                 : allpassLengthsRight[index];
            const auto length = juce::jmax (
                2, static_cast<int> (std::lround (baseLength * sampleRate / 48000.0)));

            diffusers[channel][index].buffer.setSize (1, length, false, true, false);
            diffusers[channel][index].reset();
        }
    }

    const auto preDelayLength = static_cast<int> (
        std::ceil (maximumPreDelaySeconds * sampleRate)) + 4;
    preDelayBuffer.setSize (2, preDelayLength, false, true, true);
    preDelayBuffer.clear();
    preDelayWriteIndex = 0;

    preDelaySamples.reset (sampleRate, 0.05);
    diffusionCoefficient.reset (sampleRate, 0.03);
    dampingCoefficient.reset (sampleRate, 0.05);
    modulationDepthSamples.reset (sampleRate, 0.05);
    freezeAmount.reset (sampleRate, 0.15);
    width.reset (sampleRate, 0.03);

    lowCutCoefficient = 1.0f - std::exp (-twoPi * lowCutHz / static_cast<float> (sampleRate));

    updateDerivedParameters (Parameters {});
    reset();
}

void DarkReverb::reset()
{
    for (auto& line : lines)
        line.reset();

    for (auto& channel : diffusers)
        for (auto& allpass : channel)
            allpass.reset();

    preDelayBuffer.clear();
    preDelayWriteIndex = 0;

    snapSmoothedValues();
}

void DarkReverb::setParameters (const Parameters& newParameters) noexcept
{
    updateDerivedParameters (newParameters);
}

void DarkReverb::updateDerivedParameters (const Parameters& newParameters) noexcept
{
    const auto sr = static_cast<float> (sampleRate);

    const auto damping = juce::jlimit (0.0f, 1.0f, newParameters.damping);
    const auto dampingCutoff = juce::jlimit (
        200.0f, sr * 0.45f, 20000.0f * std::pow (0.015f, damping));
    const auto dampingCoefficientTarget = juce::jlimit (
        0.0005f, 1.0f, 1.0f - std::exp (-twoPi * dampingCutoff / sr));

    diffusionCoefficient.setTargetValue (0.72f * juce::jlimit (0.0f, 1.0f, newParameters.diffusion));
    dampingCoefficient.setTargetValue (dampingCoefficientTarget);
    preDelaySamples.setTargetValue (juce::jlimit (
        0.0f,
        static_cast<float> (preDelayBuffer.getNumSamples() - 2),
        newParameters.preDelayMs * 0.001f * sr));
    modulationDepthSamples.setTargetValue (
        juce::jlimit (0.0f, 1.0f, newParameters.modulationDepth) * maximumModulationSeconds * sr);
    freezeAmount.setTargetValue (newParameters.freeze ? 1.0f : 0.0f);
    width.setTargetValue (juce::jlimit (0.0f, 2.0f, newParameters.width));

    modulationIncrement = twoPi * juce::jlimit (0.01f, 2.0f, newParameters.modulationRateHz) / sr;

    const auto decaySeconds = juce::jlimit (0.1f, 10.0f, newParameters.sizeSeconds);

    for (auto& line : lines)
    {
        const auto delaySeconds = static_cast<float> (line.baseDelaySamples) / sr;

        const auto gain = newParameters.freeze
                              ? maximumFeedbackGain
                              : juce::jmin (maximumFeedbackGain,
                                            std::pow (10.0f, -3.0f * delaySeconds / decaySeconds));

        line.feedbackGain.setTargetValue (gain);
    }
}

void DarkReverb::snapSmoothedValues() noexcept
{
    preDelaySamples.setCurrentAndTargetValue (preDelaySamples.getTargetValue());
    diffusionCoefficient.setCurrentAndTargetValue (diffusionCoefficient.getTargetValue());
    dampingCoefficient.setCurrentAndTargetValue (dampingCoefficient.getTargetValue());
    modulationDepthSamples.setCurrentAndTargetValue (modulationDepthSamples.getTargetValue());
    freezeAmount.setCurrentAndTargetValue (freezeAmount.getTargetValue());
    width.setCurrentAndTargetValue (width.getTargetValue());

    for (auto& line : lines)
        line.feedbackGain.setCurrentAndTargetValue (line.feedbackGain.getTargetValue());
}

float DarkReverb::readLine (const Line& line, float delaySamples) const noexcept
{
    const auto length = line.buffer.getNumSamples();

    if (length <= 0)
        return 0.0f;

    const auto lengthAsFloat = static_cast<float> (length);
    auto readPosition = static_cast<float> (line.writeIndex) - delaySamples;

    while (readPosition < 0.0f)
        readPosition += lengthAsFloat;

    while (readPosition >= lengthAsFloat)
        readPosition -= lengthAsFloat;

    const auto index0 = static_cast<int> (readPosition);
    const auto fraction = readPosition - static_cast<float> (index0);
    const auto index1 = index0 + 1 >= length ? 0 : index0 + 1;

    const auto* data = line.buffer.getReadPointer (0);

    return data[index0] + (data[index1] - data[index0]) * fraction;
}

float DarkReverb::readPreDelay (int channel, float delaySamples) const noexcept
{
    const auto length = preDelayBuffer.getNumSamples();

    if (length <= 0)
        return 0.0f;

    const auto lengthAsFloat = static_cast<float> (length);
    auto readPosition = static_cast<float> (preDelayWriteIndex) - delaySamples;

    while (readPosition < 0.0f)
        readPosition += lengthAsFloat;

    while (readPosition >= lengthAsFloat)
        readPosition -= lengthAsFloat;

    const auto index0 = static_cast<int> (readPosition);
    const auto fraction = readPosition - static_cast<float> (index0);
    const auto index1 = index0 + 1 >= length ? 0 : index0 + 1;

    const auto* data = preDelayBuffer.getReadPointer (channel);

    return data[index0] + (data[index1] - data[index0]) * fraction;
}

void DarkReverb::process (const juce::AudioBuffer<float>& input,
                          juce::AudioBuffer<float>& output,
                          int numSamples) noexcept
{
    juce::ScopedNoDenormals noDenormals;

    const auto outputChannels = juce::jmin (2, output.getNumChannels());

    if (outputChannels <= 0 || preDelayBuffer.getNumSamples() <= 0)
        return;

    const auto samplesToProcess = juce::jmin (numSamples,
                                              output.getNumSamples(),
                                              input.getNumSamples());

    if (samplesToProcess <= 0)
        return;

    const auto inputChannels = input.getNumChannels();
    const auto preDelayLength = preDelayBuffer.getNumSamples();

    for (int sample = 0; sample < samplesToProcess; ++sample)
    {
        const auto inputLeft = inputChannels > 0 ? input.getSample (0, sample) : 0.0f;
        const auto inputRight = inputChannels > 1 ? input.getSample (1, sample) : inputLeft;

        preDelayBuffer.setSample (0, preDelayWriteIndex, inputLeft);
        preDelayBuffer.setSample (1, preDelayWriteIndex, inputRight);

        const auto preDelay = preDelaySamples.getNextValue();
        auto diffusedLeft = readPreDelay (0, preDelay);
        auto diffusedRight = readPreDelay (1, preDelay);

        preDelayWriteIndex = wrapIndex (preDelayWriteIndex + 1, preDelayLength);

        const auto diffusion = diffusionCoefficient.getNextValue();

        for (size_t index = 0; index < diffusers[0].size(); ++index)
        {
            diffusedLeft = diffusers[0][index].process (diffusedLeft, diffusion);
            diffusedRight = diffusers[1][index].process (diffusedRight, diffusion);
        }

        std::array<float, numLines> lineOutputs {};
        const auto modulationDepth = modulationDepthSamples.getNextValue();

        for (size_t index = 0; index < lines.size(); ++index)
        {
            auto& line = lines[index];

            line.modPhase += modulationIncrement * (0.85f + 0.03f * static_cast<float> (index));

            if (line.modPhase >= twoPi)
                line.modPhase -= twoPi;

            const auto delay = juce::jmax (
                2.0f,
                static_cast<float> (line.baseDelaySamples)
                    + modulationDepth * std::sin (line.modPhase));

            lineOutputs[index] = readLine (line, delay);
        }

        std::array<float, numLines> feedback {};

        for (size_t index = 0; index < lines.size(); ++index)
        {
            auto& line = lines[index];
            auto value = lineOutputs[index];

            // Fixed low-cut keeps DC from building up in the feedback loop.
            line.lowCutState += lowCutCoefficient * (value - line.lowCutState);
            value -= line.lowCutState;

            // One-pole damping rolls off highs on every pass.
            line.dampState += dampingCoefficient.getNextValue() * (value - line.dampState);
            value = line.dampState;

            feedback[index] = value * line.feedbackGain.getNextValue();
        }

        const auto injection = 1.0f - freezeAmount.getNextValue();

        for (size_t index = 0; index < lines.size(); ++index)
        {
            const auto source = (index % 2 == 0) ? diffusedLeft : diffusedRight;
            const auto polarity = (index % 4) < 2 ? 1.0f : -1.0f;
            feedback[index] += source * injection * 0.6f * polarity;
        }

        hadamard8 (feedback);

        for (size_t index = 0; index < lines.size(); ++index)
        {
            auto& line = lines[index];

            // Safety clamp: extreme settings can never write unbounded energy.
            const auto value = juce::jlimit (-8.0f, 8.0f, feedback[index]);

            line.buffer.setSample (0, line.writeIndex, value);
            line.writeIndex = wrapIndex (line.writeIndex + 1, line.buffer.getNumSamples());
        }

        auto left = 0.5f * (lineOutputs[0] - lineOutputs[2] + lineOutputs[4] - lineOutputs[6])
                    + 0.35f * (lineOutputs[1] - lineOutputs[3]);
        auto right = 0.5f * (lineOutputs[1] - lineOutputs[3] + lineOutputs[5] - lineOutputs[7])
                     - 0.35f * (lineOutputs[0] - lineOutputs[2]);

        left *= 0.5f;
        right *= 0.5f;

        const auto stereoWidth = width.getNextValue();
        const auto mid = 0.5f * (left + right);
        const auto side = 0.5f * (left - right) * stereoWidth;

        left = softClip (mid + side);
        right = softClip (mid - side);

        output.setSample (0, sample, left);

        if (outputChannels > 1)
            output.setSample (1, sample, right);
    }
}

double DarkReverb::getMaximumTailSeconds() const noexcept
{
    return 10.0;
}
} // namespace darkgrain::dsp
