#include "dsp/GrainEngine.h"

#include <cmath>

namespace darkgrain::dsp
{
void GrainBuffer::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;

    const auto length = juce::jmax (1, static_cast<int> (std::ceil (historySeconds * sampleRate)));
    buffer.setSize (2, length, false, true, true);
    buffer.clear();

    writeGain.reset (sampleRate, 0.03);
    writeGain.setCurrentAndTargetValue (1.0f);

    writeIndex = 0;
    availableHistory = 0;
}

void GrainBuffer::reset()
{
    buffer.clear();
    writeIndex = 0;
    availableHistory = 0;
    writeGain.setCurrentAndTargetValue (1.0f);
}

void GrainBuffer::setFrozen (bool shouldFreeze) noexcept
{
    writeGain.setTargetValue (shouldFreeze ? 0.0f : 1.0f);
}

void GrainBuffer::write (const juce::AudioBuffer<float>& input, int numSamples) noexcept
{
    const auto length = buffer.getNumSamples();

    if (length <= 0 || numSamples <= 0)
        return;

    const auto inputChannels = input.getNumChannels();
    const auto samplesToWrite = juce::jmin (numSamples, input.getNumSamples(), buffer.getNumSamples());

    for (int sample = 0; sample < samplesToWrite; ++sample)
    {
        const auto gain = writeGain.getNextValue();

        // A held buffer stops advancing so the same region is recycled.
        if (gain <= 0.0001f)
            continue;

        for (int channel = 0; channel < 2; ++channel)
        {
            const auto sourceChannel = juce::jmin (channel, inputChannels - 1);

            if (sourceChannel < 0)
                continue;

            auto* data = buffer.getWritePointer (channel);
            const auto incoming = input.getSample (sourceChannel, sample);
            data[writeIndex] += (incoming - data[writeIndex]) * gain;
        }

        writeIndex = writeIndex + 1 >= length ? 0 : writeIndex + 1;

        if (availableHistory < length)
            ++availableHistory;
    }
}

float GrainBuffer::read (int channel, double position) const noexcept
{
    const auto length = buffer.getNumSamples();

    if (length <= 0 || channel < 0 || channel >= buffer.getNumChannels())
        return 0.0f;

    const auto lengthAsDouble = static_cast<double> (length);

    while (position < 0.0)
        position += lengthAsDouble;

    while (position >= lengthAsDouble)
        position -= lengthAsDouble;

    const auto index0 = static_cast<int> (position);
    const auto index1 = index0 + 1 >= length ? 0 : index0 + 1;
    const auto fraction = static_cast<float> (position - static_cast<double> (index0));

    const auto* data = buffer.getReadPointer (channel);

    return data[index0] + (data[index1] - data[index0]) * fraction;
}

void GrainEngine::prepare (double newSampleRate, double historySeconds)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;

    grainBuffer.prepare (sampleRate);

    for (int index = 0; index <= windowTableSize; ++index)
    {
        const auto phase = static_cast<float> (index) / static_cast<float> (windowTableSize);
        windowTable[static_cast<size_t> (index)] =
            0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * phase);
    }

    normalisation.reset (sampleRate, 0.05);
    normalisation.setCurrentAndTargetValue (0.9f);

    reset();

    juce::ignoreUnused (historySeconds);
}

void GrainEngine::reset()
{
    grainBuffer.reset();
    numActive = 0;
    grainPhase = 0.0;
}

void GrainEngine::setParameters (const GrainParameters& newParameters) noexcept
{
    parameters = newParameters;
    grainBuffer.setFrozen (parameters.freeze);

    const auto sizeSeconds = juce::jlimit (0.01f, 0.5f, parameters.sizeMs * 0.001f);
    const auto overlap = juce::jmax (1.0f, parameters.density * sizeSeconds);
    normalisation.setTargetValue (0.9f / std::sqrt (overlap));
}

float GrainEngine::windowAt (int age, int lengthSamples) const noexcept
{
    if (lengthSamples <= 0)
        return 0.0f;

    const auto phase = static_cast<float> (age) / static_cast<float> (lengthSamples);
    const auto index = juce::jlimit (0, windowTableSize,
                                     static_cast<int> (phase * static_cast<float> (windowTableSize)));

    return windowTable[static_cast<size_t> (index)];
}

void GrainEngine::spawnGrain() noexcept
{
    if (numActive >= maxGrains || grainBuffer.getLength() <= 0)
        return;

    const auto sizeSeconds = juce::jlimit (0.01f, 0.5f, parameters.sizeMs * 0.001f);
    const auto lengthSamples = juce::jmax (8, static_cast<int> (std::lround (sizeSeconds * sampleRate)));

    const auto pitchOffset = (random.nextFloat() * 2.0f - 1.0f)
                           * juce::jmax (0.0f, parameters.pitchScatterSemitones);
    const auto pitch = parameters.pitchSemitones + pitchOffset;
    const auto rate = static_cast<double> (std::pow (2.0, static_cast<double> (pitch) / 12.0));

    Grain grain;
    grain.lengthSamples = lengthSamples;
    grain.increment = rate;
    grain.age = 0;

    const auto readSpan = static_cast<double> (lengthSamples) * rate;
    const auto available = static_cast<double> (grainBuffer.getAvailableHistory()) - 1.0;
    const auto lookback = juce::jmax (readSpan + 0.005 * sampleRate, 0.03 * sampleRate);
    const auto usableLookback = juce::jmin (juce::jmax (0.0, available), lookback);
    const auto maxOffset = juce::jmax (0.0, available - readSpan);
    const auto spread = juce::jlimit (0.0f, 1.0f, parameters.spread);
    const auto randomOffset = static_cast<double> (spread) * maxOffset
                            * static_cast<double> (random.nextDouble());

    auto start = static_cast<double> (grainBuffer.getWritePosition())
               - usableLookback - randomOffset;

    if (random.nextFloat() < juce::jlimit (0.0f, 1.0f, parameters.reverseAmount))
    {
        start += readSpan;
        grain.increment = -rate;
    }

    const auto lengthAsDouble = static_cast<double> (grainBuffer.getLength());

    while (start < 0.0)
        start += lengthAsDouble;

    while (start >= lengthAsDouble)
        start -= lengthAsDouble;

    grain.position = start;

    const auto spreadAmount = juce::jlimit (0.0f, 1.0f, parameters.stereoSpread);
    const auto pan = (random.nextFloat() * 2.0f - 1.0f) * spreadAmount;
    const auto angle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
    grain.panLeft = std::cos (angle);
    grain.panRight = std::sin (angle);

    const auto stereoJitter = static_cast<double> (spreadAmount) * 0.002 * sampleRate;
    grain.offsetLeft = (static_cast<double> (random.nextFloat()) * 2.0 - 1.0) * stereoJitter;
    grain.offsetRight = (static_cast<double> (random.nextFloat()) * 2.0 - 1.0) * stereoJitter;

    grain.amplitude = 0.85f + 0.15f * random.nextFloat();

    grains[static_cast<size_t> (numActive)] = grain;
    ++numActive;
}

void GrainEngine::process (const juce::AudioBuffer<float>& input,
                           juce::AudioBuffer<float>& output,
                           int numSamples) noexcept
{
    juce::ScopedNoDenormals noDenormals;

    const auto outputChannels = juce::jmin (2, output.getNumChannels());
    const auto samplesToProcess = juce::jmin (numSamples, output.getNumSamples());

    if (outputChannels <= 0 || samplesToProcess <= 0)
        return;

    output.clear (0, samplesToProcess);

    grainBuffer.write (input, samplesToProcess);

    const auto density = juce::jlimit (0.0f, 128.0f, parameters.density);
    const auto increment = static_cast<double> (density) / sampleRate;

    for (int sample = 0; sample < samplesToProcess; ++sample)
    {
        grainPhase += increment;

        while (grainPhase >= 1.0)
        {
            grainPhase -= 1.0;
            spawnGrain();
        }

        const auto normaliseGain = normalisation.getNextValue();
        auto left = 0.0f;
        auto right = 0.0f;

        for (int index = 0; index < numActive;)
        {
            auto& grain = grains[static_cast<size_t> (index)];
            const auto envelope = windowAt (grain.age, grain.lengthSamples);

            left += grainBuffer.read (0, grain.position + grain.offsetLeft)
                  * envelope * grain.panLeft * grain.amplitude;
            right += grainBuffer.read (1, grain.position + grain.offsetRight)
                   * envelope * grain.panRight * grain.amplitude;

            grain.position += grain.increment;
            ++grain.age;

            if (grain.age >= grain.lengthSamples)
            {
                grains[static_cast<size_t> (index)] = grains[static_cast<size_t> (numActive - 1)];
                --numActive;
            }
            else
            {
                ++index;
            }
        }

        output.setSample (0, sample, left * normaliseGain);

        if (outputChannels > 1)
            output.setSample (1, sample, right * normaliseGain);
    }
}
} // namespace darkgrain::dsp
