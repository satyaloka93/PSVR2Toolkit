#include "vr_motion_haptics.h"

#include "common.h"
#include "haptics_engine.h"
#include "vr_motion_protocol.h"

#include <algorithm>
#include <chrono>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace psvr2_toolkit::bridge {
namespace {

constexpr const char *kMappingName = "CyberpunkVR_Hands_Shared";
constexpr size_t kMappingBytes = 1024;

using namespace vrmotion;

// Poll fast enough that a swing feels attached to the motion. At 90 Hz a frame is ~11 ms, so
// 4 ms keeps the pulse inside the frame that produced it while costing nothing measurable.
constexpr auto kPollInterval = std::chrono::milliseconds(4);

// Retry the mapping while the game is closed. The bridge is normally started first.
constexpr auto kAttachRetryInterval = std::chrono::milliseconds(1000);
// FlushHandsToShared advances the heartbeat every OpenXR frame. A one-second timeout tolerates
// loading stalls while still rejecting a stale mapping retained after Cyberpunk exits.
constexpr auto kHeartbeatTimeout = std::chrono::milliseconds(1000);

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
  // Do not trust the existing sequence yet. This mapping may have survived an earlier game process
  // because the bridge itself held the final handle open. Protocol activation requires a compatible
  // marker AND a heartbeat change from the currently running plugin.
  m_lastHeartbeat = m_shared[kSlotProtocolHeartbeat];
  m_haveHeartbeat = true;
  m_haveSequence = false;
  m_protocolReady = false;
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
  m_haveHeartbeat = false;
  m_protocolReady = false;
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
  auto heartbeatAt = std::chrono::steady_clock::now();
  auto markerInvalidSince = std::chrono::steady_clock::time_point{};
  constexpr auto kMarkerStartupGrace = std::chrono::seconds(2);

  while (m_running.load(std::memory_order_acquire)) {
    const auto now = std::chrono::steady_clock::now();
    if (!m_shared) {
      if (now >= nextAttach) {
        nextAttach = now + kAttachRetryInterval;
        if (AttachMapping()) heartbeatAt = now;
      }
      std::this_thread::sleep_for(kPollInterval);
      continue;
    }

    const bool markerValid = MarkerValid(m_shared);
    const float heartbeat = m_shared[kSlotProtocolHeartbeat];
    if (!markerValid) {
      m_protocolReady = false;
      m_haveSequence = false;
      if (markerInvalidSince == std::chrono::steady_clock::time_point{})
        markerInvalidSince = now;
      // OnPresent creates the mapping before the same first frame reaches FlushHandsToShared and
      // publishes metadata. The watcher can observe that short zero-filled window. Treat it as
      // startup, not an incompatibility, and keep expected pause notices on stdout: Windows
      // PowerShell converts a native stderr line into NativeCommandError under ErrorAction=Stop,
      // terminating run_bridge.ps1 even though the watcher would have recovered next frame.
      if (!m_incompatibleLogged && now - markerInvalidSince >= kMarkerStartupGrace) {
        std::cout << "VR motion haptics paused: incompatible CyberpunkVR shared-slot layout "
                  << "(magic=" << m_shared[kSlotProtocolMagic]
                  << ", version=" << m_shared[kSlotProtocolVersion]
                  << "); input/driving values will not be treated as pulses.\n";
        m_incompatibleLogged = true;
      }
      std::this_thread::sleep_for(kPollInterval);
      continue;
    }
    markerInvalidSince = std::chrono::steady_clock::time_point{};

    if (!m_haveHeartbeat || heartbeat != m_lastHeartbeat) {
      m_lastHeartbeat = heartbeat;
      m_haveHeartbeat = true;
      heartbeatAt = now;
      if (!m_protocolReady) {
        // Adopt sequence on activation so a record left by the previous process is never replayed.
        m_lastSequence = m_shared[kSlotSequence];
        m_haveSequence = true;
        m_protocolReady = true;
        m_staleLogged = false;
        m_incompatibleLogged = false;
        std::cout << "VR motion haptics protocol v1 active (driving-safe slot layout).\n";
      }
    } else if (now - heartbeatAt > kHeartbeatTimeout) {
      m_protocolReady = false;
      m_haveSequence = false;
      if (!m_staleLogged) {
        std::cout << "VR motion haptics paused: CyberpunkVR heartbeat is stale; gun/audio/vehicle "
                     "bridge effects remain independent.\n";
        m_staleLogged = true;
      }
    }

    if (!m_protocolReady) {
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
      if (PayloadValid(rawAmplitude, rawDuration)) {
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
