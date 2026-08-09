#pragma once

#include "common.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>

namespace psvr2_toolkit::bridge {

class HapticsEngine {
public:
  HapticsEngine() = default;
  ~HapticsEngine();

  bool Start();
  void Stop();

  // Sustained, shot-like texture. rateHz controls the repeating impact rate;
  // amplitude is normalized 0..1.
  void SetRhythm(VRControllerType controller, float amplitude, float rateHz);
  void ClearRhythm(VRControllerType controller);
  void Pulse(VRControllerType controller, float amplitude, uint32_t durationMs, float carrierHz = 135.0f);

  // Queues interleaved stereo game-audio samples already converted to 3000 Hz
  // normalized PCM. Audio is mixed with semantic weapon/vehicle effects.
  void PushGameAudio(const float *interleavedStereo, size_t frameCount);
  void ClearGameAudio();

private:
  struct Channel {
    float rhythmAmplitude = 0.0f;
    float rhythmRateHz = 0.0f;
    double rhythmPhase = 0.0;
    double carrierPhase = 0.0;
    float pulseAmplitude = 0.0f;
    float pulseCarrierHz = 135.0f;
    uint32_t pulseSamplesLeft = 0;
    uint32_t pulseSamplesTotal = 0;
  };

  void Run();
  void ForEach(VRControllerType controller, const auto &callback) {
    if (controller == VRControllerType::Left || controller == VRControllerType::Both)
      callback(m_channels[0]);
    if (controller == VRControllerType::Right || controller == VRControllerType::Both)
      callback(m_channels[1]);
  }

  std::array<Channel, 2> m_channels{};
  std::array<std::deque<float>, 2> m_gameAudio;
  std::mutex m_mutex;
  std::atomic<bool> m_running{false};
  std::thread m_thread;
};

} // namespace psvr2_toolkit::bridge
