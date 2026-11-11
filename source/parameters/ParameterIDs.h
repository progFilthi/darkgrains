#pragma once

namespace darkgrain::parameters::ids
{
// Stable automation IDs. Never change these after release.
inline constexpr const char* inputGain = "inputGain";
inline constexpr const char* highPass = "highPass";
inline constexpr const char* grainSize = "grainSize";
inline constexpr const char* grainDensity = "grainDensity";
inline constexpr const char* grainSpread = "grainSpread";
inline constexpr const char* grainPitch = "grainPitch";
inline constexpr const char* pitchScatter = "pitchScatter";
inline constexpr const char* reverseAmount = "reverseAmount";
inline constexpr const char* stereoSpread = "stereoSpread";
inline constexpr const char* freeze = "freeze";
inline constexpr const char* particleDensity = "particleDensity";
inline constexpr const char* particleBrightness = "particleBrightness";
inline constexpr const char* particleRandomness = "particleRandomness";
inline constexpr const char* particleSize = "particleSize";
inline constexpr const char* particlePitch = "particlePitch";
inline constexpr const char* particleWidth = "particleWidth";
inline constexpr const char* particleFeedback = "particleFeedback";
inline constexpr const char* particleMix = "particleMix";
inline constexpr const char* reverbSize = "reverbSize";
inline constexpr const char* reverbDamping = "reverbDamping";
inline constexpr const char* diffusion = "diffusion";
inline constexpr const char* modulationRate = "modulationRate";
inline constexpr const char* modulationDepth = "modulationDepth";
inline constexpr const char* preDelay = "preDelay";
inline constexpr const char* reverbMix = "reverbMix";
inline constexpr const char* toneDark = "toneDark";
inline constexpr const char* saturation = "saturation";
inline constexpr const char* width = "width";
inline constexpr const char* outputGain = "outputGain";
inline constexpr const char* dryWet = "dryWet";

// Musical key. Used to snap generated grain and particle pitches to the
// producer's scale so textures stay in key.
inline constexpr const char* rootKey = "rootKey";
inline constexpr const char* scale = "scale";

// Host-facing bypass parameter. Not part of the sound design surface, but it
// lets hosts automate bypass while the processor crossfades internally.
inline constexpr const char* bypass = "bypass";
} // namespace darkgrain::parameters::ids
