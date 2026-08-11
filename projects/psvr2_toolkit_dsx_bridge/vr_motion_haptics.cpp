#include "vr_motion_haptics.h"

#include "common.h"
#include "haptics_engine.h"

#include <algorithm>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

namespace psvr2_toolkit::bridge {
namespace {

constexpr const char *kMappingName = "CyberpunkVR_Hands_Shared";
constexpr size_t kMappingBytes = 1024;

constexpr int kSlotSequence = 157;
constexpr int kSlotHand = 158;
constexpr int kSlotAmplitude = 159;
constexpr int kSlotDurationMs = 160;

// Poll fast enough that a swing feels attached to the motion. At 90 Hz a frame is ~11 ms, so
// 4 ms keeps the pulse inside the frame that produced it while costing nothing measurable.
constexpr auto kPollInterval = std::chrono::milliseconds(4);

// Retry the mapping while the game is closed. The bridge is normally started first.
constexpr auto kAttachRetryInterval = std::chrono::milliseconds(1000);

// Short pulses read as a whoosh, longer ones as contact. Splitting on the plugin's own
// duration keeps the choice of carrier here rather than making the plugin know about Sense
// actuator behaviour.
constexpr uint32_t kImpactDurationThresholdMs = 60;
constexpr float kSwingCarrierHz = 150.0f;
constexpr float kImpactCarrierHz = 70.0f;

} // namespace

VRMotionHaptics::~VRMotionHaptics() { Stop(); }

bool VRMotionHaptics::AttachMapping() {
#ifdef _WIN32
  if (m_shared)
    return true;
  HANDLE handle = OpenFileMappingA(FILE_MAP_READ, FALSE, kMappingName);
  if (!handle)
    return false;
  const void *view = MapViewOfFile(handle, FILE_MAP_READ, 0, 0, kMappingBytes);
  if (!view) {
    CloseHandle(handle);
    return false;
  }
  m_mapping = handle;
  m_shared = static_cast<const float *>(view);
  // Adopt the current sequence rather than firing for whatever happened before we attached.
  m_lastSequence = m_shared[kSlotSequence];
  m_haveSequence = true;
  return true;
#else
  return false;
#endif
}

void VRMotionHaptics::DetachMapping() {
#ifdef _WIN32
  if (m_shared) {
    UnmapViewOfFile(m_shared);
    m_shared = nullptr;
  }
  if (m_mapping) {
    CloseHandle(static_cast<HANDLE>(m_mapping));
    m_mapping = nullptr;
  }
#endif
  m_haveSequence = false;
}

bool VRMotionHaptics::Start(HapticsEngine *engine, float gain) {
#ifndef _WIN32
  m_lastError = "shared-memory motion haptics are Windows-only";
  return false;
#else
  if (!engine) {
    m_lastError = "no haptics engine";
    return false;
  }
  if (m_running.load(std::memory_order_acquire))
    return true;

  m_engine = engine;
  m_gain = std::clamp(gain, 0.0f, 3.0f);
  m_running.store(true, std::memory_order_release);
  m_thread = std::thread(&VRMotionHaptics::Run, this);
  return true;
#endif
}

void VRMotionHaptics::Stop() {
  if (m_running.exchange(false, std::memory_order_acq_rel)) {
    if (m_thread.joinable())
      m_thread.join();
  }
  DetachMapping();
  m_engine = nullptr;
}

void VRMotionHaptics::Run() {
#ifdef _WIN32
  auto nextAttach = std::chrono::steady_clock::now();

  while (m_running.load(std::memory_order_acquire)) {
    if (!m_shared) {
      const auto now = std::chrono::steady_clock::now();
      if (now >= nextAttach) {
        nextAttach = now + kAttachRetryInterval;
        AttachMapping();
      }
      std::this_thread::sleep_for(kPollInterval);
      continue;
    }

    // The plugin writes the payload first and the sequence last, so a changed sequence means
    // the rest of the record is already complete. Compared as a float because that is what the
    // block stores; equality is exact for the small integers it holds.
    const float sequence = m_shared[kSlotSequence];
    if (m_haveSequence && sequence != m_lastSequence) {
      m_lastSequence = sequence;

      const float rawAmplitude = m_shared[kSlotAmplitude];
      const float rawDuration = m_shared[kSlotDurationMs];
      const bool rightHand = m_shared[kSlotHand] > 0.5f;

      // The mapping outlives the game process; a torn or stale record must not reach the
      // actuators as a maximum-strength pulse.
      if (rawAmplitude > 0.0f && rawAmplitude <= 1.0f && rawDuration >= 1.0f &&
          rawDuration <= 1000.0f) {
        const float amplitude = std::clamp(rawAmplitude * m_gain, 0.0f, 1.0f);
        const auto durationMs = static_cast<uint32_t>(rawDuration);
        const float carrierHz =
            durationMs >= kImpactDurationThresholdMs ? kImpactCarrierHz : kSwingCarrierHz;

        m_engine->Pulse(rightHand ? VRControllerType::Right : VRControllerType::Left, amplitude,
                        durationMs, carrierHz);
        m_pulses.fetch_add(1, std::memory_order_relaxed);
      }
    }

    // Cyberpunk exiting destroys the mapping; the view stays valid but stops advancing. The
    // retry path re-attaches on the next launch, so nothing here needs to detect that.
    std::this_thread::sleep_for(kPollInterval);
  }
#endif
}

} // namespace psvr2_toolkit::bridge
