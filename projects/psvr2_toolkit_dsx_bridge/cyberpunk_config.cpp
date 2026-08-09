#include "cyberpunk_config.h"

#include <charconv>
#include <sstream>
#include <string_view>
#include <unordered_map>

namespace psvr2_toolkit::cyberpunk {
namespace {

const std::unordered_map<std::string, int> kModes = {
    {"Normal", 0},          {"GameCube", 1},         {"VerySoft", 2},
    {"Soft", 3},            {"Hard", 4},             {"VeryHard", 5},
    {"Hardest", 6},         {"Rigid", 7},            {"VibrateTrigger", 8},
    {"Choppy", 9},          {"Medium", 10},          {"VibrateTriggerPulse", 11},
    {"CustomTriggerValue", 12}, {"Resistance", 13}, {"Bow", 14},
    {"Galloping", 15},      {"SemiAutomaticGun", 16}, {"AutomaticGun", 17},
    {"Machine", 18},        {"VIBRATE_TRIGGER_10Hz", 19}, {"OFF", 20},
    {"FEEDBACK", 21},       {"WEAPON", 22},          {"VIBRATION", 23},
    {"SLOPE_FEEDBACK", 24}, {"MULTIPLE_POSITION_FEEDBACK", 25},
    {"MULTIPLE_POSITION_VIBRATION", 26},
};

void SetError(std::string *error, const std::string &message) {
  if (error)
    *error = message;
}

std::vector<int> ParseValues(std::string_view value, bool &valid) {
  std::vector<int> values;
  valid = true;
  size_t position = 0;
  while ((position = value.find('(', position)) != std::string_view::npos) {
    const size_t end = value.find(')', position + 1);
    if (end == std::string_view::npos) {
      valid = false;
      return {};
    }
    const std::string_view number = value.substr(position + 1, end - position - 1);
    int parsed = 0;
    const auto result = std::from_chars(number.data(), number.data() + number.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != number.data() + number.size()) {
      valid = false;
      return {};
    }
    values.push_back(parsed);
    position = end + 1;
  }
  return values;
}

} // namespace

bool ParseConfig(const std::string &text, std::vector<dsx::Instruction> &instructions, std::string *error) {
  instructions.clear();
  std::unordered_map<std::string, std::string> values;
  std::istringstream stream(text);
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    const size_t separator = line.find('=');
    if (separator != std::string::npos)
      values[line.substr(0, separator)] = line.substr(separator + 1);
  }

  if (values["ResetToUserSettings"] == "true") {
    instructions.push_back({static_cast<int>(dsx::InstructionType::ResetToUserSettings), {0}});
    return true;
  }

  for (const auto &[side, modeKey, forceKey] : {
           std::tuple<int, const char *, const char *>{1, "LeftTrigger", "ForceLeftTrigger"},
           std::tuple<int, const char *, const char *>{2, "RightTrigger", "ForceRightTrigger"},
       }) {
    const auto mode = kModes.find(values[modeKey]);
    if (mode == kModes.end()) {
      SetError(error, std::string("missing or unknown ") + modeKey);
      instructions.clear();
      return false;
    }

    bool forceValid = false;
    std::vector<int> force = ParseValues(values[forceKey], forceValid);
    if (!forceValid) {
      SetError(error, std::string("invalid ") + forceKey);
      instructions.clear();
      return false;
    }

    dsx::Instruction instruction;
    instruction.type = static_cast<int>(dsx::InstructionType::TriggerUpdate);
    instruction.parameters = {0, side, mode->second};
    instruction.parameters.insert(instruction.parameters.end(), force.begin(), force.end());
    instructions.push_back(std::move(instruction));
  }
  return true;
}

} // namespace psvr2_toolkit::cyberpunk
