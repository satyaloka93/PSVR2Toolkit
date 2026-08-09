#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#endif

#include "cyberpunk_config.h"
#include "dsx_protocol.h"
#include "haptics_engine.h"
#include "psvr2tk_capi_loader.h"

#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif

namespace {

std::atomic<bool> g_running{true};

void StopSignal(int) { g_running = false; }

#ifdef _WIN32
BOOL WINAPI ConsoleHandler(DWORD) {
  g_running = false;
  return TRUE;
}
#endif

void CloseSocket(SocketHandle socket) {
#ifdef _WIN32
  closesocket(socket);
#else
  close(socket);
#endif
}

std::vector<std::filesystem::path> DsxPortFiles() {
  std::vector<std::filesystem::path> paths;
#ifdef _WIN32
  char localAppData[MAX_PATH] = {};
  const DWORD length = GetEnvironmentVariableA("LOCALAPPDATA", localAppData, MAX_PATH);
  if (length > 0 && length < MAX_PATH)
    paths.emplace_back(std::filesystem::path(localAppData) / "DSX" / "DSX_UDP_PortNumber.txt");
  paths.emplace_back("C:\\Temp\\DualSenseX\\DualSenseX_PortNumber.txt");
#else
  if (const char *localAppData = std::getenv("LOCALAPPDATA"))
    paths.emplace_back(std::filesystem::path(localAppData) / "DSX" / "DSX_UDP_PortNumber.txt");
#endif
  return paths;
}

void PublishPort(uint16_t port) {
  for (const auto &path : DsxPortFiles()) {
    try {
      std::filesystem::create_directories(path.parent_path());
      std::ofstream output(path, std::ios::trunc);
      output << port;
      if (output)
        std::cout << "Published DSX-compatible port file: " << path.string() << '\n';
    } catch (const std::exception &exception) {
      std::cerr << "Could not write " << path.string() << ": " << exception.what() << '\n';
    }
  }
}

struct Options {
  uint16_t port = 6969;
  std::filesystem::path cyberpunkConfig;
};

Options ResolveOptions(int argc, char **argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    if (argument == "--port" && i + 1 < argc) {
      const int port = std::stoi(argv[++i]);
      if (port <= 0 || port > 65535)
        throw std::runtime_error("port must be between 1 and 65535");
      options.port = static_cast<uint16_t>(port);
      continue;
    }
    if (argument == "--cyberpunk-config" && i + 1 < argc) {
      options.cyberpunkConfig = argv[++i];
      continue;
    }
    if (argument == "--help" || argument == "-h") {
      std::cout << "Usage: psvr2_toolkit_dsx_bridge [--port PORT] [--cyberpunk-config FILE]\n"
                   "Receives DSX UDP commands and can directly monitor Enhanced DualSense Support's config.\n";
      std::exit(0);
    }
    throw std::runtime_error("unknown argument: " + argument);
  }
  return options;
}

SocketHandle OpenServer(uint16_t port) {
#ifdef _WIN32
  WSADATA data{};
  if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
    return kInvalidSocket;
#endif

  const SocketHandle socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (socketHandle == kInvalidSocket)
    return kInvalidSocket;

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(socketHandle, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0) {
    CloseSocket(socketHandle);
    return kInvalidSocket;
  }

#ifdef _WIN32
  DWORD timeoutMs = 250;
  setsockopt(socketHandle, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeoutMs), sizeof(timeoutMs));
#else
  timeval timeout{0, 250000};
  setsockopt(socketHandle, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
  return socketHandle;
}

std::string ServerResponse() {
  return "{\"Status\":\"PSVR2 Toolkit bridge\",\"TimeReceived\":\"now\",\"isControllerConnected\":true,\"BatteryLevel\":100,\"Devices\":["
         "{\"Index\":0,\"MacAddress\":\"\",\"DeviceType\":5,\"BatteryLevel\":100,\"IsSupportAT\":true,\"IsSupportLightBar\":false,"
         "\"IsSupportPlayerLED\":false,\"IsSupportMicLED\":false},"
         "{\"Index\":1,\"MacAddress\":\"\",\"DeviceType\":6,\"BatteryLevel\":100,\"IsSupportAT\":true,\"IsSupportLightBar\":false,"
         "\"IsSupportPlayerLED\":false,\"IsSupportMicLED\":false}]}";
}

void ResetTriggers() {
  ScePadTriggerEffectCommand off{};
  off.mode = SCE_PAD_TRIGGER_EFFECT_MODE_OFF;
  psvr2_toolkit_set_trigger_effect(VRControllerType::Both, off);
}

int InstructionParam(const psvr2_toolkit::dsx::Instruction &instruction, size_t index, int fallback = 0) {
  return index < instruction.parameters.size() ? instruction.parameters[index] : fallback;
}

struct HapticTriggerState {
  bool valid = false;
  int mode = 0;
  std::vector<int> parameters;
};

std::array<bool, 2> UpdateCyberpunkHaptics(const std::vector<psvr2_toolkit::dsx::Instruction> &instructions,
                                           std::array<HapticTriggerState, 2> &states,
                                           psvr2_toolkit::bridge::HapticsEngine &haptics) {
  std::array<bool, 2> firingBreaks{};
  for (const auto &instruction : instructions) {
    if (instruction.type != static_cast<int>(psvr2_toolkit::dsx::InstructionType::TriggerUpdate) || instruction.parameters.size() < 3)
      continue;
    const int sideValue = instruction.parameters[1];
    if (sideValue < 1 || sideValue > 2)
      continue;
    const size_t sideIndex = static_cast<size_t>(sideValue - 1);
    const VRControllerType controller = sideValue == 1 ? VRControllerType::Left : VRControllerType::Right;
    HapticTriggerState &previous = states[sideIndex];
    const int mode = instruction.parameters[2];
    const bool changed = !previous.valid || previous.mode != mode || previous.parameters != instruction.parameters;

    float rhythmAmplitude = 0.0f;
    float rhythmRate = 0.0f;
    if (mode == static_cast<int>(psvr2_toolkit::dsx::TriggerMode::Machine)) {
      rhythmAmplitude = std::clamp(std::max(InstructionParam(instruction, 5), InstructionParam(instruction, 6)) / 8.0f, 0.45f, 1.0f);
      rhythmRate = static_cast<float>(std::max(1, InstructionParam(instruction, 7, 10)));
    } else if (mode == static_cast<int>(psvr2_toolkit::dsx::TriggerMode::AutomaticGun) ||
               mode == static_cast<int>(psvr2_toolkit::dsx::TriggerMode::Vibration)) {
      rhythmAmplitude = std::clamp(InstructionParam(instruction, 4, 6) / 8.0f, 0.45f, 1.0f);
      rhythmRate = static_cast<float>(std::max(1, InstructionParam(instruction, 5, 10)));
    } else if (mode == static_cast<int>(psvr2_toolkit::dsx::TriggerMode::Galloping)) {
      rhythmAmplitude = 0.72f;
      rhythmRate = static_cast<float>(std::max(1, InstructionParam(instruction, 7, 6)));
    } else if (mode == static_cast<int>(psvr2_toolkit::dsx::TriggerMode::VibrateTrigger) ||
               mode == static_cast<int>(psvr2_toolkit::dsx::TriggerMode::VibrateTriggerPulse) ||
               mode == static_cast<int>(psvr2_toolkit::dsx::TriggerMode::VibrateTrigger10Hz) ||
               mode == static_cast<int>(psvr2_toolkit::dsx::TriggerMode::MultiplePositionVibration)) {
      rhythmAmplitude = 0.68f;
      rhythmRate = mode == static_cast<int>(psvr2_toolkit::dsx::TriggerMode::VibrateTrigger10Hz) ? 10.0f : 18.0f;
    }

    if (rhythmAmplitude > 0.0f)
      haptics.SetRhythm(controller, rhythmAmplitude, rhythmRate);
    else
      haptics.ClearRhythm(controller);

    if (changed && previous.valid) {
      const bool previousWasLoadedWeapon = previous.mode == static_cast<int>(psvr2_toolkit::dsx::TriggerMode::Bow) ||
                                           previous.mode == static_cast<int>(psvr2_toolkit::dsx::TriggerMode::Weapon) ||
                                           previous.mode == static_cast<int>(psvr2_toolkit::dsx::TriggerMode::SemiAutomaticGun);
      if (previousWasLoadedWeapon && mode == static_cast<int>(psvr2_toolkit::dsx::TriggerMode::Resistance)) {
        firingBreaks[sideIndex] = true;
        const float profileStrength = static_cast<float>(previous.parameters.size() > 5 ? previous.parameters[5] : 4);
        const float snapForce = static_cast<float>(previous.parameters.size() > 6 ? previous.parameters[6] : 4);
        const float impact = std::clamp(0.52f + (2.0f * profileStrength + snapForce) / 24.0f * 0.30f, 0.52f, 0.82f);
        haptics.Pulse(controller, impact, 105, 125.0f);
        // A smaller pulse in the support hand makes long guns feel two-handed.
        haptics.Pulse(sideValue == 2 ? VRControllerType::Left : VRControllerType::Right, impact * 0.52f, 75, 115.0f);
      } else if (mode == static_cast<int>(psvr2_toolkit::dsx::TriggerMode::SemiAutomaticGun)) {
        haptics.Pulse(controller, 0.68f, 55, 170.0f);
      }
    }

    previous = {true, mode, instruction.parameters};
  }
  return firingBreaks;
}

void ApplyInstructions(const std::vector<psvr2_toolkit::dsx::Instruction> &instructions, uint64_t &translatedCount,
                       const std::array<bool, 2> *firingBreaks = nullptr) {
  for (const auto &instruction : instructions) {
    if (instruction.type == static_cast<int>(psvr2_toolkit::dsx::InstructionType::ResetToUserSettings)) {
      ResetTriggers();
      continue;
    }
    if (instruction.type != static_cast<int>(psvr2_toolkit::dsx::InstructionType::TriggerUpdate))
      continue;

    psvr2_toolkit::dsx::Translation translation;
    std::string translationError;
    if (!psvr2_toolkit::dsx::TranslateTriggerUpdate(instruction, translation, &translationError)) {
      std::cerr << "Ignored trigger command: " << translationError << '\n';
      continue;
    }

    const int sideValue = InstructionParam(instruction, 1);
    if (firingBreaks && sideValue >= 1 && sideValue <= 2 && (*firingBreaks)[static_cast<size_t>(sideValue - 1)]) {
      // The best PSVR2 gun implementations ramp and plateau while pulling,
      // then drop resistance at the actual shot. Enhanced DualSense Support
      // signals that edge by changing a loaded Bow/Weapon profile to
      // Resistance. Release the motor until the mod restores the loaded curve.
      translation.command = {};
      translation.command.mode = SCE_PAD_TRIGGER_EFFECT_MODE_OFF;
      translation.description += " -> firing release";
      translation.exact = false;
    }

    psvr2_toolkit_set_trigger_effect(translation.controllerType, translation.command);
    ++translatedCount;
    const char *side = translation.controllerType == VRControllerType::Left ? "L2" : "R2";
    std::cout << '[' << translatedCount << "] " << side << " <- " << translation.description
              << (translation.exact ? "" : " (safe approximation)") << std::endl;
  }
}

void PublishCyberpunkStatus(const std::filesystem::path &configPath) {
  const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
  try {
    std::ofstream output(configPath.parent_path() / "DSXData.json", std::ios::trunc);
    output << "{\n"
              "  \"isControllerConnected\": true,\n"
              "  \"isDSXLaunched\": true,\n"
              "  \"isGameLaunched\": true,\n"
              "  \"isOldDSX\": false,\n"
              "  \"batteryLevel\": 100,\n"
              "  \"isCharging\": false,\n"
              "  \"saveDate\": "
           << now << "\n}\n";
  } catch (const std::exception &exception) {
    std::cerr << "Could not publish Cyberpunk bridge status: " << exception.what() << '\n';
  }
}

} // namespace

int main(int argc, char **argv) {
  Options options;
  try {
    options = ResolveOptions(argc, argv);
  } catch (const std::exception &exception) {
    std::cerr << "Argument error: " << exception.what() << '\n';
    return 2;
  }

  void *module = psvr2_toolkit_loader_get_module_handle();
  if (!module) {
    std::cerr << "Could not locate psvr2_toolkit_capi. Start SteamVR with PSVR2 Toolkit installed first.\n";
    return 3;
  }
  psvr2_toolkit_loader_init_functions(module);
  if (!p_psvr2_toolkit_init || !p_psvr2_toolkit_deinit || !p_psvr2_toolkit_set_trigger_effect) {
    std::cerr << "Installed PSVR2 Toolkit CAPI does not expose the required functions.\n";
    return 3;
  }

  const int initResult = psvr2_toolkit_init();
  if (initResult != PSVR2TK_RESULT_OK) {
    std::cerr << "PSVR2 Toolkit CAPI initialization failed: " << initResult << '\n';
    return 4;
  }

  const SocketHandle server = OpenServer(options.port);
  if (server == kInvalidSocket) {
    std::cerr << "Could not bind UDP 127.0.0.1:" << options.port << ". Close DSX or choose another --port.\n";
    psvr2_toolkit_deinit();
    return 5;
  }

  PublishPort(options.port);
  psvr2_toolkit::bridge::HapticsEngine haptics;
  if (haptics.Start())
    std::cout << "Cyberpunk grip PCM haptics enabled.\n";
  else
    std::cerr << "PCM haptics unavailable in this Toolkit CAPI build.\n";
  std::signal(SIGINT, StopSignal);
  std::signal(SIGTERM, StopSignal);
#ifdef _WIN32
  SetConsoleCtrlHandler(ConsoleHandler, TRUE);
#endif

  std::cout << "PSVR2 Toolkit DSX bridge listening on 127.0.0.1:" << options.port << "\n"
               "Start Cyberpunk 2077 with Enhanced DualSense Support. Do not run DSX at the same time.\n"
               "Press Ctrl+C to stop and release the trigger effects.\n";
  if (!options.cyberpunkConfig.empty())
    std::cout << "Direct Cyberpunk config monitor: " << options.cyberpunkConfig.string() << std::endl;

  uint64_t packetCount = 0;
  uint64_t translatedCount = 0;
  std::filesystem::file_time_type cyberpunkWriteTime{};
  bool cyberpunkConfigApplied = false;
  std::array<HapticTriggerState, 2> hapticStates{};
  auto lastStatusWrite = std::chrono::steady_clock::time_point{};
  while (g_running) {
    if (!options.cyberpunkConfig.empty()) {
      std::error_code fileError;
      const auto writeTime = std::filesystem::last_write_time(options.cyberpunkConfig, fileError);
      if (!fileError && (!cyberpunkConfigApplied || writeTime != cyberpunkWriteTime)) {
        std::ifstream input(options.cyberpunkConfig);
        std::ostringstream contents;
        contents << input.rdbuf();
        std::vector<psvr2_toolkit::dsx::Instruction> fileInstructions;
        std::string configError;
        if (input && psvr2_toolkit::cyberpunk::ParseConfig(contents.str(), fileInstructions, &configError)) {
          const auto firingBreaks = UpdateCyberpunkHaptics(fileInstructions, hapticStates, haptics);
          ApplyInstructions(fileInstructions, translatedCount, &firingBreaks);
          cyberpunkWriteTime = writeTime;
          cyberpunkConfigApplied = true;
        }
      }

      const auto now = std::chrono::steady_clock::now();
      if (now - lastStatusWrite >= std::chrono::seconds(1)) {
        PublishCyberpunkStatus(options.cyberpunkConfig);
        lastStatusWrite = now;
      }
    }
    char buffer[65536];
    sockaddr_in client{};
#ifdef _WIN32
    int clientLength = sizeof(client);
    const int received = recvfrom(server, buffer, static_cast<int>(sizeof(buffer) - 1), 0, reinterpret_cast<sockaddr *>(&client), &clientLength);
#else
    socklen_t clientLength = sizeof(client);
    const int received = static_cast<int>(recvfrom(server, buffer, sizeof(buffer) - 1, 0, reinterpret_cast<sockaddr *>(&client), &clientLength));
#endif
    if (received <= 0)
      continue;
    if (ntohl(client.sin_addr.s_addr) != INADDR_LOOPBACK)
      continue;

    ++packetCount;
    buffer[received] = '\0';
    std::vector<psvr2_toolkit::dsx::Instruction> instructions;
    std::string parseError;
    if (!psvr2_toolkit::dsx::ParsePacket(std::string(buffer, static_cast<size_t>(received)), instructions, &parseError)) {
      std::cerr << "Ignored malformed DSX packet: " << parseError << '\n';
      continue;
    }

    ApplyInstructions(instructions, translatedCount);

    // DSX replies to each valid datagram. The bundled Cyberpunk UDP client uses
    // these replies to publish its connected/controller state in DSXData.json.
    const std::string response = ServerResponse();
    sendto(server, response.data(), static_cast<int>(response.size()), 0, reinterpret_cast<const sockaddr *>(&client), clientLength);
  }

  std::cout << "Stopping after " << packetCount << " packets and " << translatedCount << " trigger updates.\n";
  haptics.Stop();
  ResetTriggers();
  psvr2_toolkit_deinit();
  CloseSocket(server);
#ifdef _WIN32
  WSACleanup();
#endif
  return 0;
}
