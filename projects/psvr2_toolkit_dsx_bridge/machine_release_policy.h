#pragma once

#include "dsx_protocol.h"

namespace psvr2_toolkit::bridge {

// The producer deduplicates a sustained Machine profile. Replacing its first
// update with OFF can leave the trigger motor disabled for the entire burst.
// Keep the shot-onset grip pulse, but let the Machine motor command through.
constexpr bool ShouldReleaseMotorOnFiringBreak(bool firingBreak, int nextMode) {
  return firingBreak && nextMode != static_cast<int>(dsx::TriggerMode::Machine);
}

} // namespace psvr2_toolkit::bridge
