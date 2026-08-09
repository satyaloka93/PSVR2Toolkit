#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace psvr2_toolkit::bridge {

class HapticsEngine;

// Captures the Windows default render endpoint in loopback mode and converts
// game audio to a tactile 3000 Hz stereo signal. SetEnabled gates output so
// desktop audio is never sent while Cyberpunk is not running.
class AudioHapticsCapture {
public:
  AudioHapticsCapture() = default;
  ~AudioHapticsCapture();

  bool Start(HapticsEngine *engine, float gain = 1.0f);
  void Stop();
  void SetEnabled(bool enabled);
  const std::string &LastError() const { return m_error; }

private:
  void Run();

  HapticsEngine *m_engine = nullptr;
  float m_gain = 1.0f;
  std::atomic<bool> m_running{false};
  std::atomic<bool> m_enabled{false};
  std::thread m_thread;
  std::mutex m_readyMutex;
  std::condition_variable m_readyCondition;
  bool m_ready = false;
  bool m_available = false;
  std::string m_error;
};

} // namespace psvr2_toolkit::bridge
