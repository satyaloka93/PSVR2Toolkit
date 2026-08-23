#pragma once

#include <cstddef>

namespace psvr2_toolkit::bridge::vrmotion {

// Versioned layout inside CyberpunkVR_Hands_Shared. Keep these in lockstep with
// CyberpunkVR Port's include/Utils/SharedSlots.hpp.
constexpr std::size_t kSlotSequence = 157;
constexpr std::size_t kSlotHand = 158;
constexpr std::size_t kSlotAmplitude = 159;
constexpr std::size_t kSlotDurationMs = 160;
constexpr std::size_t kSlotProtocolMagic = 168;
constexpr std::size_t kSlotProtocolVersion = 169;
constexpr std::size_t kSlotProtocolHeartbeat = 170;
constexpr float kProtocolMagic = 18512.0f; // 0x4850, "HP"
constexpr float kProtocolVersion = 1.0f;

inline bool MarkerValid(const float *shared) {
  return shared && shared[kSlotProtocolMagic] == kProtocolMagic &&
         shared[kSlotProtocolVersion] == kProtocolVersion;
}

inline bool PayloadValid(float amplitude, float durationMs) {
  return amplitude > 0.0f && amplitude <= 1.0f && durationMs >= 1.0f && durationMs <= 1000.0f;
}

} // namespace psvr2_toolkit::bridge::vrmotion
