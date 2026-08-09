#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "haptics_engine.h"

#include "psvr2tk_capi_loader.h"

#include <algorithm>
#include <cmath>

namespace psvr2_toolkit::bridge {
namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kSampleRate = 3000.0;
constexpr size_t kChunkSize = 32;

unsigned char ToPcmByte(double sample) {
  const int value = std::clamp(static_cast<int>(std::lround(sample * 127.0)), -127, 127);
  return static_cast<unsigned char>(static_cast<int8_t>(value));
}
} // namespace

HapticsEngine::~HapticsEngine() { Stop(); }

bool HapticsEngine::Start() {
  if (!p_psvr2_toolkit_wait_for_pcm || !p_psvr2_toolkit_write_pcm)
    return false;
  if (m_running.exchange(true))
    return true;
  m_thread = std::thread(&HapticsEngine::Run, this);
  return true;
}

void HapticsEngine::Stop() {
  if (!m_running.exchange(false))
    return;
  if (m_thread.joinable())
    m_thread.join();
}

void HapticsEngine::SetRhythm(VRControllerType controller, float amplitude, float rateHz) {
  std::scoped_lock lock(m_mutex);
  ForEach(controller, [&](Channel &channel) {
    channel.rhythmAmplitude = std::clamp(amplitude, 0.0f, 1.0f);
    channel.rhythmRateHz = std::clamp(rateHz, 1.0f, 40.0f);
  });
}

void HapticsEngine::ClearRhythm(VRControllerType controller) {
  std::scoped_lock lock(m_mutex);
  ForEach(controller, [](Channel &channel) {
    channel.rhythmAmplitude = 0.0f;
    channel.rhythmRateHz = 0.0f;
  });
}

void HapticsEngine::Pulse(VRControllerType controller, float amplitude, uint32_t durationMs, float carrierHz) {
  std::scoped_lock lock(m_mutex);
  ForEach(controller, [&](Channel &channel) {
    const uint32_t samples = std::max<uint32_t>(1, durationMs * 3);
    if (amplitude >= channel.pulseAmplitude || channel.pulseSamplesLeft == 0) {
      channel.pulseAmplitude = std::clamp(amplitude, 0.0f, 1.0f);
      channel.pulseCarrierHz = std::clamp(carrierHz, 60.0f, 300.0f);
      channel.pulseSamplesLeft = samples;
      channel.pulseSamplesTotal = samples;
    }
  });
}

void HapticsEngine::Run() {
  while (m_running) {
    psvr2_toolkit_wait_for_pcm();
    if (!m_running)
      break;

    std::array<unsigned char, kChunkSize> left{};
    std::array<unsigned char, kChunkSize> right{};
    bool active[2] = {false, false};

    {
      std::scoped_lock lock(m_mutex);
      for (size_t side = 0; side < m_channels.size(); ++side) {
        Channel &channel = m_channels[side];
        auto &buffer = side == 0 ? left : right;
        active[side] = channel.rhythmAmplitude > 0.0f || channel.pulseSamplesLeft > 0;
        if (!active[side])
          continue;

        for (size_t i = 0; i < kChunkSize; ++i) {
          double sample = 0.0;
          if (channel.rhythmAmplitude > 0.0f) {
            // A resonant carrier with a narrow attack envelope gives distinct
            // automatic-fire impacts instead of a featureless low buzz.
            const double envelopeBase = std::max(0.0, std::sin(channel.rhythmPhase));
            const double envelope = 0.30 + 0.70 * std::pow(envelopeBase, 6.0);
            // Sense haptics need an overdriven carrier. Toolkit's native
            // OpenVR path clips its oscillator by roughly 11-25x; a plain sine
            // is substantially weaker even at the same nominal peak value.
            const double carrier = std::clamp(std::sin(channel.carrierPhase) * 12.0, -1.0, 1.0);
            sample += carrier * channel.rhythmAmplitude * envelope;
            channel.rhythmPhase = std::fmod(channel.rhythmPhase + 2.0 * kPi * channel.rhythmRateHz / kSampleRate, 2.0 * kPi);
            channel.carrierPhase = std::fmod(channel.carrierPhase + 2.0 * kPi * 165.0 / kSampleRate, 2.0 * kPi);
          }

          if (channel.pulseSamplesLeft > 0) {
            const double progress = 1.0 - static_cast<double>(channel.pulseSamplesLeft) / channel.pulseSamplesTotal;
            const double envelope = std::exp(-2.1 * progress);
            const double carrier = std::clamp(std::sin(channel.carrierPhase) * 14.0, -1.0, 1.0);
            sample += carrier * channel.pulseAmplitude * envelope;
            channel.carrierPhase = std::fmod(channel.carrierPhase + 2.0 * kPi * channel.pulseCarrierHz / kSampleRate, 2.0 * kPi);
            --channel.pulseSamplesLeft;
            if (channel.pulseSamplesLeft == 0)
              channel.pulseAmplitude = 0.0f;
          }
          buffer[i] = ToPcmByte(std::clamp(sample, -1.0, 1.0));
        }
      }
    }

    if (active[0])
      psvr2_toolkit_write_pcm(VRControllerType::Left, left.data());
    if (active[1])
      psvr2_toolkit_write_pcm(VRControllerType::Right, right.data());
  }
}

} // namespace psvr2_toolkit::bridge
