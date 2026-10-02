#include "vr_motion_protocol.h"

#include <array>
#include <iostream>

using namespace psvr2_toolkit::bridge::vrmotion;

int main() {
  std::array<float, 256> shared{};
  if (MarkerValid(shared.data())) return 1;
  shared[kSlotProtocolMagic] = kProtocolMagic;
  if (MarkerValid(shared.data())) return 2;
  shared[kSlotProtocolVersion] = kProtocolVersion;
  if (!MarkerValid(shared.data())) return 3;
  if (!PayloadValid(0.68f, 36.0f) || PayloadValid(1.1f, 36.0f) ||
      PayloadValid(0.68f, 0.0f)) return 4;
  std::cout << "Versioned Cyberpunk motion channel accepted; invalid payload rejected\n";
  return 0;
}
