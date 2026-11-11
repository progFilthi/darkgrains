#pragma once

#include <cstdint>
#include <cmath>
#include <initializer_list>

namespace darkgrain::dsp
{
/**
    Musical scale awareness for the grain and particle engines.

    Generated pitch shifts are snapped to the selected scale so a texture stays
    in the key of the source material instead of drifting into chromatic
    dissonance.
*/
namespace scales
{
inline constexpr std::uint16_t chromatic = 0x0fff;

inline constexpr std::uint16_t maskForIndex (int index) noexcept
{
    switch (index)
    {
        case 1: // Natural minor
            return (1u << 0) | (1u << 2) | (1u << 3) | (1u << 5)
                 | (1u << 7) | (1u << 8) | (1u << 10);
        case 2: // Major
            return (1u << 0) | (1u << 2) | (1u << 4) | (1u << 5)
                 | (1u << 7) | (1u << 9) | (1u << 11);
        case 3: // Dorian
            return (1u << 0) | (1u << 2) | (1u << 3) | (1u << 5)
                 | (1u << 7) | (1u << 9) | (1u << 10);
        case 4: // Phrygian
            return (1u << 0) | (1u << 1) | (1u << 3) | (1u << 5)
                 | (1u << 7) | (1u << 8) | (1u << 10);
        case 5: // Lydian
            return (1u << 0) | (1u << 2) | (1u << 4) | (1u << 6)
                 | (1u << 7) | (1u << 9) | (1u << 11);
        case 6: // Mixolydian
            return (1u << 0) | (1u << 2) | (1u << 4) | (1u << 5)
                 | (1u << 7) | (1u << 9) | (1u << 10);
        case 7: // Harmonic minor
            return (1u << 0) | (1u << 2) | (1u << 3) | (1u << 5)
                 | (1u << 7) | (1u << 8) | (1u << 11);
        case 8: // Pentatonic minor
            return (1u << 0) | (1u << 3) | (1u << 5) | (1u << 7) | (1u << 10);
        case 9: // Pentatonic major
            return (1u << 0) | (1u << 2) | (1u << 4) | (1u << 7) | (1u << 9);
        case 0:
        default:
            return chromatic;
    }
}

/**
    Snaps a semitone offset to the nearest interval that belongs to the scale.

    The offset is relative to the source key, so 0 always stays 0. When two
    intervals are equally close the lower one is chosen.
*/
inline int quantizeSemitones (float semitones, std::uint16_t mask) noexcept
{
    if (mask == chromatic)
        return static_cast<int> (std::lround (semitones));

    const auto rounded = static_cast<int> (std::lround (semitones));

    for (int distance = 0; distance <= 12; ++distance)
    {
        for (int direction : { -1, 1 })
        {
            const auto candidate = rounded + direction * distance;
            const auto pitchClass = ((candidate % 12) + 12) % 12;

            if ((mask & (1u << pitchClass)) != 0u)
                return candidate;
        }
    }

    return rounded;
}
} // namespace scales
} // namespace darkgrain::dsp
