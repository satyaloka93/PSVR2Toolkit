#include "audio_haptics_policy.h"

#include <cmath>

int main() {
  using psvr2_toolkit::bridge::SharedLowFrequencyRoadSignal;
  if (SharedLowFrequencyRoadSignal(0.0, 0.4) != 0.4) return 1; // right-panned -> both grips
  if (SharedLowFrequencyRoadSignal(-0.4, 0.0) != -0.4) return 2; // left-panned -> both grips
  if (SharedLowFrequencyRoadSignal(0.4, -0.4) != 0.4) return 3; // no phase cancellation
  if (std::abs(SharedLowFrequencyRoadSignal(0.1, -0.3) + 0.3) >= 1e-9) return 4;
  return 0;
}
