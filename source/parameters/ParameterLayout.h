#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace darkgrain::parameters
{
/**
    Builds the complete, stable parameter tree for DarkGrain.

    Every parameter is created here once, at processor construction time, so the
    audio thread only ever reads preallocated atomic values.
*/
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
} // namespace darkgrain::parameters
