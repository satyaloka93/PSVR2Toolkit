#pragma once

#include <charconv>
#include <sstream>
#include <string>
#include <string_view>

namespace psvr2_toolkit::bridge {

// Shared on-disk contract with the PSVR2 Haptics CET companion. Keep separate
// from EDS settings.json: its settings writer must not erase these controls.
struct AudioHapticsSettings {
  bool enabled = false;
  int strength = 35;
  float Gain() const { return static_cast<float>(strength) / 100.0f; }
};

inline bool ParseAudioHapticsSettings(const std::string &text,
                                     AudioHapticsSettings &settings) {
  if (text.size() > 4096)
    return false;
  AudioHapticsSettings parsed;
  unsigned fields = 0;
  std::istringstream lines(text);
  std::string line;
  while (std::getline(lines, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty())
      continue;
    const auto separator = line.find('=');
    if (separator == std::string::npos)
      return false;
    const auto key = line.substr(0, separator);
    const std::string_view value(line.data() + separator + 1,
                                 line.size() - separator - 1);
    int number = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), number);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size())
      return false;
    unsigned bit = 0;
    if (key == "Version" && number == 1) {
      bit = 1;
    } else if (key == "AudioEnabled" && (number == 0 || number == 1)) {
      bit = 2;
      parsed.enabled = number == 1;
    } else if (key == "AudioStrength" && number >= 0 && number <= 300) {
      bit = 4;
      parsed.strength = number;
    } else {
      return false;
    }
    if (fields & bit)
      return false;
    fields |= bit;
  }
  if (fields != 7)
    return false;
  settings = parsed; // Partial/truncated writes never change the last valid state.
  return true;
}

} // namespace psvr2_toolkit::bridge
