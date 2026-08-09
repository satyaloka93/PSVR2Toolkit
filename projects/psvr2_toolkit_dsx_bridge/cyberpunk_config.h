#pragma once

#include "dsx_protocol.h"

#include <string>
#include <vector>

namespace psvr2_toolkit::cyberpunk {

// Parses Enhanced DualSense Support's generated DualSenseXConfig.txt into the
// two DSX trigger instructions consumed by the bridge.
bool ParseConfig(const std::string &text, std::vector<dsx::Instruction> &instructions, std::string *error = nullptr);

} // namespace psvr2_toolkit::cyberpunk
