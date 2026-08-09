#include "dsx_protocol.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace psvr2_toolkit::dsx {
namespace {

template <typename T> T ClampInt(int value, int minimum, int maximum) {
  return static_cast<T>(std::clamp(value, minimum, maximum));
}

int Param(const std::vector<int> &parameters, size_t index, int fallback = 0) {
  return index < parameters.size() ? parameters[index] : fallback;
}

uint8_t Strength(int value) {
  if (value > 8)
    value = static_cast<int>(std::lround(static_cast<double>(value) * 8.0 / 255.0));
  return ClampInt<uint8_t>(value, 0, 8);
}

uint8_t Position(int value, int maximum = 9) { return ClampInt<uint8_t>(value, 0, maximum); }

uint8_t Frequency(int value, int fallback = 20) {
  if (value <= 0)
    value = fallback;
  return ClampInt<uint8_t>(value, 1, 255);
}

void SetFeedback(ScePadTriggerEffectCommand &command, int position, int strength) {
  command = {};
  command.mode = SCE_PAD_TRIGGER_EFFECT_MODE_FEEDBACK;
  command.commandData.feedbackParam.position = Position(position);
  command.commandData.feedbackParam.strength = Strength(strength);
}

void SetWeapon(ScePadTriggerEffectCommand &command, int start, int end, int strength) {
  command = {};
  command.mode = SCE_PAD_TRIGGER_EFFECT_MODE_WEAPON;
  const int safeStart = std::clamp(start, 2, 7);
  const int safeEnd = std::clamp(end, safeStart + 1, 8);
  command.commandData.weaponParam.startPosition = static_cast<uint8_t>(safeStart);
  command.commandData.weaponParam.endPosition = static_cast<uint8_t>(safeEnd);
  command.commandData.weaponParam.strength = Strength(strength);
}

void SetVibration(ScePadTriggerEffectCommand &command, int position, int amplitude, int frequency) {
  command = {};
  command.mode = SCE_PAD_TRIGGER_EFFECT_MODE_VIBRATION;
  command.commandData.vibrationParam.position = Position(position);
  command.commandData.vibrationParam.amplitude = Strength(amplitude);
  command.commandData.vibrationParam.frequency = Frequency(frequency);
}

void SetSlope(ScePadTriggerEffectCommand &command, int start, int end, int startStrength, int endStrength) {
  command = {};
  command.mode = SCE_PAD_TRIGGER_EFFECT_MODE_SLOPE_FEEDBACK;
  const int safeStart = std::clamp(start, 0, 8);
  const int safeEnd = std::clamp(end, safeStart + 1, 9);
  command.commandData.slopeFeedbackParam.startPosition = static_cast<uint8_t>(safeStart);
  command.commandData.slopeFeedbackParam.endPosition = static_cast<uint8_t>(safeEnd);
  command.commandData.slopeFeedbackParam.startStrength = std::max<uint8_t>(1, Strength(startStrength));
  command.commandData.slopeFeedbackParam.endStrength = std::max<uint8_t>(1, Strength(endStrength));
}

void SetOff(ScePadTriggerEffectCommand &command) {
  command = {};
  command.mode = SCE_PAD_TRIGGER_EFFECT_MODE_OFF;
}

void SetSenseBowCurve(ScePadTriggerEffectCommand &command, int start, int end, int strength, int snapForce) {
  // DualSense mode 0x22 and Sense mode 0x22 do not share a parameter
  // layout. Feeding the DualSense packed strength pair to Sense turns a
  // normal 4/4 handgun profile into force byte 0x1b, which feels extremely
  // stiff. Preserve the profile semantics with a gradual Sense slope instead:
  // low take-up, increasing resistance, then a plateau until the shot event.
  const int semanticStrength = std::clamp((2 * std::clamp(strength, 1, 8) + std::clamp(snapForce, 1, 8) + 1) / 3, 1, 8);
  const int endStrength = std::clamp(1 + semanticStrength / 2, 1, 5);
  const int startStrength = std::max(1, (endStrength + 1) / 3);
  SetSlope(command, start, end, startStrength, endStrength);
}

bool FindMatching(const std::string &text, size_t open, char openChar, char closeChar, size_t &close) {
  int depth = 0;
  bool inString = false;
  bool escaped = false;
  for (size_t i = open; i < text.size(); ++i) {
    const char c = text[i];
    if (inString) {
      if (escaped) {
        escaped = false;
      } else if (c == '\\') {
        escaped = true;
      } else if (c == '"') {
        inString = false;
      }
      continue;
    }
    if (c == '"') {
      inString = true;
    } else if (c == openChar) {
      ++depth;
    } else if (c == closeChar && --depth == 0) {
      close = i;
      return true;
    }
  }
  return false;
}

bool ParseIntegerAfterKey(const std::string &object, const char *key, int &value) {
  const size_t keyPosition = object.find(key);
  if (keyPosition == std::string::npos)
    return false;
  size_t position = object.find(':', keyPosition + std::strlen(key));
  if (position == std::string::npos)
    return false;
  ++position;
  while (position < object.size() && std::isspace(static_cast<unsigned char>(object[position])))
    ++position;
  if (position < object.size() && object[position] == '"')
    ++position;
  char *end = nullptr;
  const long parsed = std::strtol(object.c_str() + position, &end, 10);
  if (end == object.c_str() + position || parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max())
    return false;
  value = static_cast<int>(parsed);
  return true;
}

bool ParseParameters(const std::string &object, std::vector<int> &parameters) {
  const size_t keyPosition = object.find("\"parameters\"");
  if (keyPosition == std::string::npos)
    return false;
  const size_t open = object.find('[', keyPosition);
  if (open == std::string::npos)
    return false;
  size_t close = 0;
  if (!FindMatching(object, open, '[', ']', close))
    return false;

  size_t position = open + 1;
  while (position < close) {
    while (position < close && (std::isspace(static_cast<unsigned char>(object[position])) || object[position] == ','))
      ++position;
    if (position >= close)
      break;
    if (object.compare(position, 4, "true") == 0) {
      parameters.push_back(1);
      position += 4;
      continue;
    }
    if (object.compare(position, 5, "false") == 0 || object.compare(position, 4, "null") == 0) {
      parameters.push_back(0);
      position += object[position] == 'f' ? 5 : 4;
      continue;
    }
    if (object[position] == '"')
      ++position;
    char *end = nullptr;
    const double parsed = std::strtod(object.c_str() + position, &end);
    if (end == object.c_str() + position)
      return false;
    if (parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max())
      return false;
    parameters.push_back(static_cast<int>(std::lround(parsed)));
    position = static_cast<size_t>(end - object.c_str());
    while (position < close && object[position] != ',')
      ++position;
  }
  return true;
}

void SetMultipleFeedback(ScePadTriggerEffectCommand &command, const std::vector<int> &values, size_t offset) {
  command = {};
  command.mode = SCE_PAD_TRIGGER_EFFECT_MODE_MULTIPLE_POSITION_FEEDBACK;
  for (size_t i = 0; i < SCE_PAD_TRIGGER_EFFECT_CONTROL_POINT_NUM; ++i)
    command.commandData.multiplePositionFeedbackParam.strength[i] = Strength(Param(values, offset + i));
}

void SetMultipleVibration(ScePadTriggerEffectCommand &command, const std::vector<int> &values, size_t offset, int frequency) {
  command = {};
  command.mode = SCE_PAD_TRIGGER_EFFECT_MODE_MULTIPLE_POSITION_VIBRATION;
  command.commandData.multiplePositionVibrationParam.frequency = Frequency(frequency);
  for (size_t i = 0; i < SCE_PAD_TRIGGER_EFFECT_CONTROL_POINT_NUM; ++i)
    command.commandData.multiplePositionVibrationParam.amplitude[i] = Strength(Param(values, offset + i));
}

} // namespace

bool ParsePacket(const std::string &json, std::vector<Instruction> &instructions, std::string *error) {
  instructions.clear();
  const size_t instructionsKey = json.find("\"instructions\"");
  const size_t arrayOpen = instructionsKey == std::string::npos ? std::string::npos : json.find('[', instructionsKey);
  size_t arrayClose = 0;
  if (arrayOpen == std::string::npos || !FindMatching(json, arrayOpen, '[', ']', arrayClose)) {
    if (error)
      *error = "missing or unterminated instructions array";
    return false;
  }

  size_t position = arrayOpen + 1;
  while (position < arrayClose) {
    const size_t objectOpen = json.find('{', position);
    if (objectOpen == std::string::npos || objectOpen >= arrayClose)
      break;
    size_t objectClose = 0;
    if (!FindMatching(json, objectOpen, '{', '}', objectClose) || objectClose > arrayClose) {
      if (error)
        *error = "unterminated instruction object";
      return false;
    }

    const std::string object = json.substr(objectOpen, objectClose - objectOpen + 1);
    Instruction instruction;
    if (!ParseIntegerAfterKey(object, "\"type\"", instruction.type) || !ParseParameters(object, instruction.parameters)) {
      if (error)
        *error = "instruction requires numeric type and parameters";
      return false;
    }
    instructions.push_back(std::move(instruction));
    position = objectClose + 1;
  }

  if (instructions.empty()) {
    if (error)
      *error = "packet contained no instructions";
    return false;
  }
  return true;
}

bool TranslateTriggerUpdate(const Instruction &instruction, Translation &translation, std::string *error) {
  if (instruction.type != static_cast<int>(InstructionType::TriggerUpdate)) {
    if (error)
      *error = "not a TriggerUpdate instruction";
    return false;
  }
  if (instruction.parameters.size() < 3) {
    if (error)
      *error = "TriggerUpdate requires controller, trigger and mode";
    return false;
  }

  const int trigger = instruction.parameters[1];
  if (trigger == static_cast<int>(Trigger::Left))
    translation.controllerType = VRControllerType::Left;
  else if (trigger == static_cast<int>(Trigger::Right))
    translation.controllerType = VRControllerType::Right;
  else {
    if (error)
      *error = "invalid trigger side";
    return false;
  }

  const auto &p = instruction.parameters;
  const int mode = p[2];
  translation = {translation.controllerType, {}, false, TriggerModeName(mode)};
  switch (static_cast<TriggerMode>(mode)) {
  case TriggerMode::Normal:
  case TriggerMode::Off:
    SetOff(translation.command);
    translation.exact = true;
    break;
  case TriggerMode::VerySoft:
    SetFeedback(translation.command, 2, 1);
    break;
  case TriggerMode::Soft:
    SetFeedback(translation.command, 2, 2);
    break;
  case TriggerMode::Medium:
    SetFeedback(translation.command, 2, 4);
    break;
  case TriggerMode::Hard:
    SetFeedback(translation.command, 2, 5);
    break;
  case TriggerMode::VeryHard:
    SetFeedback(translation.command, 1, 7);
    break;
  case TriggerMode::Hardest:
  case TriggerMode::Rigid:
    SetFeedback(translation.command, 0, 8);
    break;
  case TriggerMode::GameCube:
    SetFeedback(translation.command, 5, 5);
    break;
  case TriggerMode::Choppy: {
    std::vector<int> strengths = {1, 7, 1, 7, 1, 7, 1, 7, 1, 7};
    SetMultipleFeedback(translation.command, strengths, 0);
    break;
  }
  case TriggerMode::VibrateTrigger:
    SetVibration(translation.command, 0, Strength(Param(p, 3, 128)), 20);
    break;
  case TriggerMode::VibrateTriggerPulse:
    SetVibration(translation.command, 2, Strength(Param(p, 3, 255)), 10);
    break;
  case TriggerMode::VibrateTrigger10Hz:
    SetVibration(translation.command, 0, Strength(Param(p, 3, 255)), 10);
    break;
  case TriggerMode::Resistance:
  case TriggerMode::Feedback:
    SetFeedback(translation.command, Param(p, 3), Param(p, 4));
    translation.exact = true;
    break;
  case TriggerMode::Bow:
    SetSenseBowCurve(translation.command, Param(p, 3), Param(p, 4, 8), Param(p, 5, 8), Param(p, 6, 8));
    break;
  case TriggerMode::SlopeFeedback:
    SetSlope(translation.command, Param(p, 3), Param(p, 4, 8), Param(p, 5, 2), Param(p, 6, 5));
    translation.exact = true;
    break;
  case TriggerMode::SemiAutomaticGun:
  case TriggerMode::Weapon:
    SetWeapon(translation.command, Param(p, 3, 2), Param(p, 4, 7), Param(p, 5, 8));
    translation.exact = true;
    break;
  case TriggerMode::AutomaticGun:
  case TriggerMode::Vibration:
    SetVibration(translation.command, Param(p, 3), Param(p, 4, 8), Param(p, 5, 10));
    translation.exact = true;
    break;
  case TriggerMode::Galloping: {
    std::vector<int> amplitudes(10, 0);
    const int start = std::clamp(Param(p, 3), 0, 9);
    const int end = std::clamp(Param(p, 4, 9), start, 9);
    const int amplitude = std::clamp(std::max(Param(p, 5, 2), Param(p, 6, 4)) + 1, 1, 8);
    for (int i = start; i <= end; ++i)
      amplitudes[i] = amplitude;
    SetMultipleVibration(translation.command, amplitudes, 0, Param(p, 7, 10));
    break;
  }
  case TriggerMode::Machine: {
    std::vector<int> amplitudes(10, 0);
    const int start = std::clamp(Param(p, 3), 0, 9);
    const int end = std::clamp(Param(p, 4, 9), start, 9);
    const int amplitude = std::clamp((Param(p, 5, 7) + Param(p, 6, 4) + 1) / 2, 1, 8);
    for (int i = start; i <= end; ++i)
      amplitudes[i] = amplitude;
    SetMultipleVibration(translation.command, amplitudes, 0, Param(p, 7, 10));
    break;
  }
  case TriggerMode::CustomTriggerValue: {
    const int customMode = Param(p, 3);
    if (customMode == 0) {
      SetOff(translation.command);
    } else if (customMode <= 4) {
      SetFeedback(translation.command, Param(p, 4), Param(p, 5, 255));
    } else if (customMode <= 8) {
      SetWeapon(translation.command, Param(p, 4, 2), Param(p, 5, 7), Param(p, 6, 255));
    } else {
      SetVibration(translation.command, Param(p, 4), Param(p, 5, 255), Param(p, 6, 10));
    }
    break;
  }
  case TriggerMode::MultiplePositionFeedback:
    SetMultipleFeedback(translation.command, p, 3);
    translation.exact = true;
    break;
  case TriggerMode::MultiplePositionVibration:
    SetMultipleVibration(translation.command, p, 4, Param(p, 3, 10));
    translation.exact = true;
    break;
  default:
    if (error)
      *error = "unsupported DSX trigger mode " + std::to_string(mode);
    return false;
  }

  return true;
}

const char *TriggerModeName(int mode) {
  static constexpr const char *names[] = {"Normal",          "GameCube",        "VerySoft",          "Soft",       "Hard",       "VeryHard", "Hardest",
                                           "Rigid",           "VibrateTrigger",  "Choppy",            "Medium",     "VibrateTriggerPulse", "CustomTriggerValue",
                                           "Resistance",      "Bow",             "Galloping",         "SemiAutomaticGun", "AutomaticGun", "Machine",
                                           "VibrateTrigger10Hz", "Off",          "Feedback",          "Weapon",     "Vibration", "SlopeFeedback",
                                           "MultiplePositionFeedback", "MultiplePositionVibration"};
  return mode >= 0 && mode < static_cast<int>(sizeof(names) / sizeof(names[0])) ? names[mode] : "Unknown";
}

} // namespace psvr2_toolkit::dsx
