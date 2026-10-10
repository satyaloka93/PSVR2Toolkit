#include "audio_haptics_settings.h"
#include "audio_haptics_policy.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

int main() {
  using namespace psvr2_toolkit::bridge;
  auto check = [](bool passed, const char *message) {
    if (!passed) throw std::runtime_error(message);
  };
  AudioHapticsSettings settings;
  check(!settings.enabled && settings.strength == 35, "safe defaults");
  check(ParseAudioHapticsSettings("Version=1\nAudioEnabled=1\nAudioStrength=70\n", settings), "valid enabled");
  check(settings.enabled && std::abs(settings.Gain() - 0.7f) < 0.0001f, "live gain");
  for (const auto *invalid : {"", "Version=1\n", "Version=1\nAudioEnabled=1\nAudioStrength=",
      "Version=2\nAudioEnabled=1\nAudioStrength=35\n", "Version=1\nAudioEnabled=2\nAudioStrength=35\n",
      "Version=1\nAudioEnabled=1\nAudioStrength=-1\n", "Version=1\nAudioEnabled=1\nAudioStrength=301\n",
      "Version=1\nAudioEnabled=1\nAudioStrength=NaN\n", "Version=1\nAudioEnabled=1\nAudioStrength=1.5\n",
      "Version=1\nAudioEnabled=1\nAudioStrength=35tail\n", "Version=1\nAudioEnabled=1\nAudioStrength=35\nAudioEnabled=0\n",
      "Version=1\nAudioEnabled=1\nAudioStrength=35\nUnknown=1\n"}) {
    check(!ParseAudioHapticsSettings(invalid, settings), "reject invalid settings");
    check(settings.enabled && settings.strength == 70, "retain last complete selection");
  }
  check(!ParseAudioHapticsSettings(std::string(5000, 'x'), settings), "bounded input");
  check(ParseAudioHapticsSettings("Version=1\r\nAudioEnabled=0\r\nAudioStrength=300\r\n", settings), "CRLF/off");
  check(!settings.enabled && settings.Gain() == 3.0f, "off retains strength");
  check(ParseAudioHapticsSettings("AudioStrength=0\nAudioEnabled=1\nVersion=1\n", settings), "order independent zero");
  check(settings.enabled && settings.Gain() == 0, "zero gain");
  check(SharedLowFrequencyRoadSignal(0, .4) == .4, "right bass to both grips");
  check(SharedLowFrequencyRoadSignal(-.4, 0) == -.4, "left bass to both grips");
  check(SharedLowFrequencyRoadSignal(.4, -.4) == .4, "no stereo cancellation");
  std::cout << "Audio settings and road-rumble regressions passed.\n";
}
