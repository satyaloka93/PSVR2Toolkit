#include "machine_release_policy.h"
#include "cyberpunk_config.h"

#include <iostream>
#include <string>
#include <vector>

using namespace psvr2_toolkit;

static_assert(!bridge::ShouldReleaseMotorOnFiringBreak(true, static_cast<int>(dsx::TriggerMode::Machine)));
static_assert(bridge::ShouldReleaseMotorOnFiringBreak(true, static_cast<int>(dsx::TriggerMode::Resistance)));
static_assert(bridge::ShouldReleaseMotorOnFiringBreak(true, static_cast<int>(dsx::TriggerMode::Bow)));
static_assert(!bridge::ShouldReleaseMotorOnFiringBreak(false, static_cast<int>(dsx::TriggerMode::Resistance)));

int main() {
  const std::string config =
      "LeftTrigger=Choppy\n"
      "ForceLeftTrigger=(0)(0)(0)(0)(0)(0)(0)\n"
      "RightTrigger=Machine\n"
      "ForceRightTrigger=(2)(9)(4)(5)(12)(0)\n"
      "ResetToUserSettings=false\n";
  std::vector<dsx::Instruction> instructions;
  std::string error;
  if (!cyberpunk::ParseConfig(config, instructions, &error) || instructions.size() != 2) {
    std::cerr << "Saratoga config parse failed: " << error << '\n';
    return 1;
  }
  const auto &right = instructions[1];
  if (right.parameters.size() != 9 || right.parameters[2] != static_cast<int>(dsx::TriggerMode::Machine) ||
      bridge::ShouldReleaseMotorOnFiringBreak(true, right.parameters[2])) {
    std::cerr << "Machine profile was discarded at shot onset\n";
    return 1;
  }
  dsx::Translation translated;
  if (!dsx::TranslateTriggerUpdate(right, translated, &error) ||
      translated.controllerType != VRControllerType::Right ||
      translated.command.mode != SCE_PAD_TRIGGER_EFFECT_MODE_MULTIPLE_POSITION_VIBRATION ||
      translated.command.commandData.multiplePositionVibrationParam.frequency != 12 ||
      translated.command.commandData.multiplePositionVibrationParam.amplitude[2] != 5 ||
      translated.command.commandData.multiplePositionVibrationParam.amplitude[9] != 5) {
    std::cerr << "Machine profile did not reach Sense vibration: " << error << '\n';
    return 1;
  }
  std::cout << "Machine firing profile survives shot release\n";
  return 0;
}
