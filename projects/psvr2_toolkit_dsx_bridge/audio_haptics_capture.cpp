#include "audio_haptics_capture.h"

#include "haptics_engine.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <audioclient.h>
#include <ks.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <windows.h>
#endif

namespace psvr2_toolkit::bridge {
namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kHapticSampleRate = 3000.0;

#ifdef _WIN32
bool IsFloatFormat(const WAVEFORMATEX *format) {
  if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
    return true;
  if (format->wFormatTag != WAVE_FORMAT_EXTENSIBLE || format->cbSize < 22)
    return false;
  return reinterpret_cast<const WAVEFORMATEXTENSIBLE *>(format)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
}

bool IsPcmFormat(const WAVEFORMATEX *format) {
  if (format->wFormatTag == WAVE_FORMAT_PCM)
    return true;
  if (format->wFormatTag != WAVE_FORMAT_EXTENSIBLE || format->cbSize < 22)
    return false;
  return reinterpret_cast<const WAVEFORMATEXTENSIBLE *>(format)->SubFormat == KSDATAFORMAT_SUBTYPE_PCM;
}

float ReadSample(const BYTE *frame, uint16_t channel, const WAVEFORMATEX *format) {
  const uint16_t bytesPerSample = static_cast<uint16_t>(format->wBitsPerSample / 8);
  const BYTE *sample = frame + static_cast<size_t>(channel) * bytesPerSample;
  if (IsFloatFormat(format) && format->wBitsPerSample == 32)
    return std::clamp(*reinterpret_cast<const float *>(sample), -1.0f, 1.0f);
  if (!IsPcmFormat(format))
    return 0.0f;
  switch (format->wBitsPerSample) {
  case 8:
    return (static_cast<int>(*sample) - 128) / 128.0f;
  case 16:
    return *reinterpret_cast<const int16_t *>(sample) / 32768.0f;
  case 24: {
    int32_t value = static_cast<int32_t>(sample[0]) | (static_cast<int32_t>(sample[1]) << 8) | (static_cast<int32_t>(sample[2]) << 16);
    if (value & 0x800000)
      value |= ~0xFFFFFF;
    return value / 8388608.0f;
  }
  case 32:
    return static_cast<float>(*reinterpret_cast<const int32_t *>(sample) / 2147483648.0);
  default:
    return 0.0f;
  }
}
#endif
} // namespace

AudioHapticsCapture::~AudioHapticsCapture() { Stop(); }

bool AudioHapticsCapture::Start(HapticsEngine *engine, float gain) {
  if (!engine || m_running.exchange(true))
    return engine != nullptr;
  m_engine = engine;
  m_gain = std::clamp(gain, 0.0f, 3.0f);
  m_ready = false;
  m_available = false;
  m_error.clear();
  m_thread = std::thread(&AudioHapticsCapture::Run, this);
  std::unique_lock lock(m_readyMutex);
  m_readyCondition.wait_for(lock, std::chrono::seconds(3), [&] { return m_ready; });
  return m_available;
}

void AudioHapticsCapture::Stop() {
  if (!m_running.exchange(false))
    return;
  if (m_thread.joinable())
    m_thread.join();
  if (m_engine)
    m_engine->ClearGameAudio();
}

void AudioHapticsCapture::SetEnabled(bool enabled) {
  m_enabled = enabled;
  if (!enabled && m_engine)
    m_engine->ClearGameAudio();
}

void AudioHapticsCapture::Run() {
#ifndef _WIN32
  {
    std::scoped_lock lock(m_readyMutex);
    m_error = "game-audio haptics require Windows WASAPI";
    m_ready = true;
  }
  m_readyCondition.notify_all();
  m_running = false;
#else
  const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  IMMDeviceEnumerator *enumerator = nullptr;
  IMMDevice *device = nullptr;
  IAudioClient *audioClient = nullptr;
  IAudioCaptureClient *captureClient = nullptr;
  WAVEFORMATEX *format = nullptr;

  auto fail = [&](const char *message, HRESULT result) {
    char code[32] = {};
    std::snprintf(code, sizeof(code), " (HRESULT 0x%08lx)", static_cast<unsigned long>(result));
    std::scoped_lock lock(m_readyMutex);
    m_error = std::string(message) + code;
  };

  HRESULT result = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
  if (SUCCEEDED(result))
    result = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device);
  if (SUCCEEDED(result))
    result = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(&audioClient));
  if (SUCCEEDED(result))
    result = audioClient->GetMixFormat(&format);
  if (SUCCEEDED(result) && !IsFloatFormat(format) && !IsPcmFormat(format))
    result = AUDCLNT_E_UNSUPPORTED_FORMAT;
  if (SUCCEEDED(result))
    result = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, format, nullptr);
  if (SUCCEEDED(result))
    result = audioClient->GetService(IID_PPV_ARGS(&captureClient));
  if (SUCCEEDED(result))
    result = audioClient->Start();

  if (FAILED(result)) {
    fail("could not initialize default-output loopback capture", result);
  } else {
    std::scoped_lock lock(m_readyMutex);
    m_available = true;
  }
  {
    std::scoped_lock lock(m_readyMutex);
    m_ready = true;
  }
  m_readyCondition.notify_all();

  if (SUCCEEDED(result)) {
    const double rate = static_cast<double>(format->nSamplesPerSec);
    const double hpAlpha = std::exp(-2.0 * kPi * 28.0 / rate);
    const double lpAlpha = 1.0 - std::exp(-2.0 * kPi * 320.0 / rate);
    std::array<double, 2> previousInput{};
    std::array<double, 2> highPassed{};
    std::array<double, 2> lowPassed{};
    double fastEnvelope = 0.0;
    double slowEnvelope = 0.0;
    double resampleAccumulator = 0.0;
    double tactilePhase = 0.0;

    while (m_running) {
      UINT32 packetFrames = 0;
      result = captureClient->GetNextPacketSize(&packetFrames);
      if (FAILED(result))
        break;
      if (packetFrames == 0) {
        Sleep(3);
        continue;
      }

      while (packetFrames > 0 && m_running) {
        BYTE *data = nullptr;
        UINT32 frames = 0;
        DWORD flags = 0;
        result = captureClient->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
        if (FAILED(result))
          break;
        const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
        std::vector<float> tactile;
        tactile.reserve(static_cast<size_t>(frames) * 2 * 3000 / std::max<uint32_t>(1, format->nSamplesPerSec) + 4);

        for (UINT32 frameIndex = 0; frameIndex < frames; ++frameIndex) {
          std::array<double, 2> input{};
          if (!silent) {
            const BYTE *frame = data + static_cast<size_t>(frameIndex) * format->nBlockAlign;
            input[0] = ReadSample(frame, 0, format);
            input[1] = format->nChannels > 1 ? ReadSample(frame, 1, format) : input[0];
            for (uint16_t channel = 2; channel < format->nChannels; ++channel) {
              const double extra = ReadSample(frame, channel, format) * 0.12;
              input[0] += extra;
              input[1] += extra;
            }
          }

          for (size_t side = 0; side < 2; ++side) {
            highPassed[side] = hpAlpha * (highPassed[side] + input[side] - previousInput[side]);
            previousInput[side] = input[side];
            lowPassed[side] += lpAlpha * (highPassed[side] - lowPassed[side]);
          }
          const double fullEnergy = std::max(std::abs(input[0]), std::abs(input[1]));
          fastEnvelope += 0.08 * (fullEnergy - fastEnvelope);
          slowEnvelope += 0.0012 * (fullEnergy - slowEnvelope);

          resampleAccumulator += kHapticSampleRate;
          if (resampleAccumulator < rate)
            continue;
          resampleAccumulator -= rate;

          const double transient = std::clamp((fastEnvelope - slowEnvelope * 1.12) * 15.0, 0.0, 1.0);
          const double carrier = std::clamp(std::sin(tactilePhase) * 10.0, -1.0, 1.0);
          tactilePhase = std::fmod(tactilePhase + 2.0 * kPi * 155.0 / kHapticSampleRate, 2.0 * kPi);
          for (size_t side = 0; side < 2; ++side) {
            const double bassEnergy = std::abs(lowPassed[side]);
            const double gate = std::clamp((bassEnergy - 0.0015) / 0.006, 0.0, 1.0);
            const double bass = std::tanh(lowPassed[side] * 13.0) * gate;
            const double output = std::tanh((bass * 0.72 + carrier * transient * 0.92) * m_gain);
            tactile.push_back(static_cast<float>(output));
          }
        }

        captureClient->ReleaseBuffer(frames);
        if (m_enabled && !tactile.empty())
          m_engine->PushGameAudio(tactile.data(), tactile.size() / 2);
        result = captureClient->GetNextPacketSize(&packetFrames);
        if (FAILED(result))
          break;
      }
      if (FAILED(result))
        break;
    }
    audioClient->Stop();
    if (FAILED(result) && m_running)
      fail("game-audio loopback capture stopped", result);
  }

  if (format)
    CoTaskMemFree(format);
  if (captureClient)
    captureClient->Release();
  if (audioClient)
    audioClient->Release();
  if (device)
    device->Release();
  if (enumerator)
    enumerator->Release();
  if (SUCCEEDED(comResult))
    CoUninitialize();
#endif
}

} // namespace psvr2_toolkit::bridge
