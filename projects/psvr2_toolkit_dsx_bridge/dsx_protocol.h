#pragma once

#include "common.h"
#include "pad_trigger_effect.h"

#include <string>
#include <vector>

namespace psvr2_toolkit::dsx {

enum class InstructionType : int {
  Invalid = 0,
  TriggerUpdate = 1,
  RGBUpdate = 2,
  PlayerLED = 3,
  TriggerThreshold = 4,
  MicLED = 5,
  PlayerLEDNewRevision = 6,
  ResetToUserSettings = 7,
};

enum class Trigger : int { Invalid = 0, Left = 1, Right = 2 };

enum class TriggerMode : int {
  Normal = 0,
  GameCube = 1,
  VerySoft = 2,
  Soft = 3,
  Hard = 4,
  VeryHard = 5,
  Hardest = 6,
  Rigid = 7,
  VibrateTrigger = 8,
  Choppy = 9,
  Medium = 10,
  VibrateTriggerPulse = 11,
  CustomTriggerValue = 12,
  Resistance = 13,
  Bow = 14,
  Galloping = 15,
  SemiAutomaticGun = 16,
  AutomaticGun = 17,
  Machine = 18,
  VibrateTrigger10Hz = 19,
  Off = 20,
  Feedback = 21,
  Weapon = 22,
  Vibration = 23,
  SlopeFeedback = 24,
  MultiplePositionFeedback = 25,
  MultiplePositionVibration = 26,
};

struct Instruction {
  int type = 0;
  std::vector<int> parameters;
};

struct Translation {
  VRControllerType controllerType = VRControllerType::Both;
  ScePadTriggerEffectCommand command{};
  bool exact = false;
  std::string description;
};

// Parses the numeric subset of the DSX UDP JSON protocol. Unknown fields and
// non-trigger instructions are retained/ignored rather than rejecting a packet.
bool ParsePacket(const std::string &json, std::vector<Instruction> &instructions, std::string *error = nullptr);

// Converts a DSX TriggerUpdate instruction to one of the official trigger
// commands accepted by PSVR2 Toolkit. Legacy DSX convenience modes are mapped
// to the closest safe official effect; DSX modes 20-26 map directly.
bool TranslateTriggerUpdate(const Instruction &instruction, Translation &translation, std::string *error = nullptr);

const char *TriggerModeName(int mode);

} // namespace psvr2_toolkit::dsx
