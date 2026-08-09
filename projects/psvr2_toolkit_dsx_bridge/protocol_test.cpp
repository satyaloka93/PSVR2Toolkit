#include "cyberpunk_config.h"
#include "dsx_protocol.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace psvr2_toolkit::dsx;

int main() {
  std::vector<Instruction> instructions;
  std::string error;
  const std::string packet =
      R"({"instructions":[{"type":1,"parameters":[0,2,13,3,8]},{"type":1,"parameters":[0,1,22,2,7,6]},{"type":7,"parameters":[0]}]})";
  assert(ParsePacket(packet, instructions, &error));
  assert(instructions.size() == 3);

  Translation resistance;
  assert(TranslateTriggerUpdate(instructions[0], resistance, &error));
  assert(resistance.controllerType == VRControllerType::Right);
  assert(resistance.command.mode == SCE_PAD_TRIGGER_EFFECT_MODE_FEEDBACK);
  assert(resistance.command.commandData.feedbackParam.position == 3);
  assert(resistance.command.commandData.feedbackParam.strength == 8);
  assert(resistance.exact);

  Translation weapon;
  assert(TranslateTriggerUpdate(instructions[1], weapon, &error));
  assert(weapon.controllerType == VRControllerType::Left);
  assert(weapon.command.mode == SCE_PAD_TRIGGER_EFFECT_MODE_WEAPON);
  assert(weapon.command.commandData.weaponParam.startPosition == 2);
  assert(weapon.command.commandData.weaponParam.endPosition == 7);
  assert(weapon.command.commandData.weaponParam.strength == 6);
  assert(weapon.exact);

  Instruction bow{1, {0, 2, 14, 1, 4, 7, 4}};
  Translation bowApproximation;
  assert(TranslateTriggerUpdate(bow, bowApproximation, &error));
  assert(bowApproximation.command.mode == SCE_PAD_TRIGGER_EFFECT_MODE_SLOPE_FEEDBACK);
  assert(bowApproximation.command.commandData.slopeFeedbackParam.startPosition == 1);
  assert(bowApproximation.command.commandData.slopeFeedbackParam.endPosition == 4);
  assert(bowApproximation.command.commandData.slopeFeedbackParam.startStrength == 1);
  assert(bowApproximation.command.commandData.slopeFeedbackParam.endStrength == 4);
  assert(!bowApproximation.exact);

  Instruction galloping{1, {0, 1, 15, 2, 8, 3, 5, 14}};
  Translation gallopingRaw;
  assert(TranslateTriggerUpdate(galloping, gallopingRaw, &error));
  assert(gallopingRaw.command.mode == SCE_PAD_TRIGGER_EFFECT_MODE_MULTIPLE_POSITION_VIBRATION);
  assert(gallopingRaw.command.commandData.multiplePositionVibrationParam.frequency == 14);
  assert(gallopingRaw.command.commandData.multiplePositionVibrationParam.amplitude[1] == 0);
  assert(gallopingRaw.command.commandData.multiplePositionVibrationParam.amplitude[2] == 6);
  assert(gallopingRaw.command.commandData.multiplePositionVibrationParam.amplitude[8] == 6);
  assert(gallopingRaw.command.commandData.multiplePositionVibrationParam.amplitude[9] == 0);

  Instruction machine{1, {0, 2, 16, 1, 9, 7, 4, 18, 6}};
  Translation machineRaw;
  assert(TranslateTriggerUpdate(machine, machineRaw, &error));
  assert(machineRaw.command.mode == SCE_PAD_TRIGGER_EFFECT_MODE_MULTIPLE_POSITION_VIBRATION);
  assert(machineRaw.command.commandData.multiplePositionVibrationParam.frequency == 18);
  assert(machineRaw.command.commandData.multiplePositionVibrationParam.amplitude[0] == 0);
  assert(machineRaw.command.commandData.multiplePositionVibrationParam.amplitude[1] == 6);
  assert(machineRaw.command.commandData.multiplePositionVibrationParam.amplitude[9] == 6);

  Instruction automatic{1, {0, 2, 17, 1, 255, 12}};
  Translation vibration;
  assert(TranslateTriggerUpdate(automatic, vibration, &error));
  assert(vibration.command.mode == SCE_PAD_TRIGGER_EFFECT_MODE_VIBRATION);
  assert(vibration.command.commandData.vibrationParam.position == 1);
  assert(vibration.command.commandData.vibrationParam.amplitude == 8);
  assert(vibration.command.commandData.vibrationParam.frequency == 12);

  Instruction directMulti{1, {0, 1, 25, 0, 1, 2, 3, 4, 5, 6, 7, 8, 0}};
  Translation multi;
  assert(TranslateTriggerUpdate(directMulti, multi, &error));
  assert(multi.command.mode == SCE_PAD_TRIGGER_EFFECT_MODE_MULTIPLE_POSITION_FEEDBACK);
  assert(multi.command.commandData.multiplePositionFeedbackParam.strength[7] == 7);
  assert(multi.command.commandData.multiplePositionFeedbackParam.strength[8] == 8);

  assert(!ParsePacket("{}", instructions, &error));
  assert(!TranslateTriggerUpdate(Instruction{1, {0, 7, 13}}, resistance, &error));

  const std::string cyberpunkConfig =
      "LeftTrigger=Resistance\n"
      "RightTrigger=Bow\n"
      "ForceLeftTrigger=(0)(3)\n"
      "ForceRightTrigger=(1)(4)(5)(4)\n"
      "ResetToUserSettings=false\n";
  assert(psvr2_toolkit::cyberpunk::ParseConfig(cyberpunkConfig, instructions, &error));
  assert(instructions.size() == 2);
  assert(instructions[0].parameters == std::vector<int>({0, 1, 13, 0, 3}));
  assert(instructions[1].parameters == std::vector<int>({0, 2, 14, 1, 4, 5, 4}));

  std::cout << "DSX protocol tests passed\n";
  return 0;
}
