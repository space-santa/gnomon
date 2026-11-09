// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <algorithm>
#include <cstdint>

namespace euclid
{
/** True if `step` is a hit in a Euclidean rhythm that spreads `pulses` hits as evenly
    as possible over `steps` steps, shifted later in time by `rotation` steps.

    `step` is an absolute step count (e.g. since the start of the song). It wraps, and
    may be negative (some hosts report negative positions during pre-roll).
*/
inline bool isHit (std::int64_t step, int steps, int pulses, int rotation) noexcept
{
    if (steps <= 0)
        return false;

    pulses = std::clamp (pulses, 0, steps);

    if (pulses == 0)
        return false;

    const auto i = ((step - rotation) % steps + steps) % steps;
    return (i * pulses) % steps < pulses;
}
} // namespace euclid
