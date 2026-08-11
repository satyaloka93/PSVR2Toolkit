#pragma once

#include <atomic>
#include <string>
#include <thread>

namespace psvr2_toolkit::bridge {

class HapticsEngine;

// Motion-driven haptics from the CyberpunkVR Port VR plugin.
//
// The audio-derived layer cannot reproduce a melee swing: the weapon whoosh sits outside the
// 28-320 Hz tactile band and is inaudible to it even at high gain, and being stereo-derived it
// buzzes both grips rather than the weapon hand. The plugin already detects swings and impacts
// with speed thresholds and per-episode edge detection, and publishes them into the shared
// memory block it uses to talk to its own modules. This watcher consumes that.
//
// It must live on its own thread. The bridge's main loop can idle up to 250 ms on SO_RCVTIMEO
// waiting for UDP, which is far too slow for a swing to feel connected to the motion.
//
// Layout of "CyberpunkVR_Hands_Shared" (1024 bytes / 256 floats) used here:
//   [157] sequence   -- incremented by the plugin AFTER the payload is written
//   [158] hand       -- 0 = left, 1 = right
//   [159] amplitude  -- 0..1
//   [160] duration   -- milliseconds
class VRMotionHaptics {
public:
  ~VRMotionHaptics();

  // Never fails hard: the mapping only exists while Cyberpunk is running with the plugin
  // loaded, and the bridge must stay useful without it. Returns false when the watcher did
  // not start; LastError() explains why.
  bool Start(HapticsEngine *engine, float gain = 1.0f);
  void Stop();

  const std::string &LastError() const { return m_lastError; }
  uint64_t PulsesDelivered() const { return m_pulses.load(std::memory_order_relaxed); }

private:
  void Run();
  bool AttachMapping();
  void DetachMapping();

  HapticsEngine *m_engine = nullptr;
  float m_gain = 1.0f;
  void *m_mapping = nullptr;      // HANDLE
  const float *m_shared = nullptr;
  float m_lastSequence = 0.0f;
  bool m_haveSequence = false;
  std::string m_lastError;
  std::atomic<uint64_t> m_pulses{0};
  std::atomic<bool> m_running{false};
  std::thread m_thread;
};

} // namespace psvr2_toolkit::bridge
