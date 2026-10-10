#pragma once

#include <cmath>

namespace psvr2_toolkit::bridge {

// Preserve the strongest bass channel without averaging away stereo phase differences.
inline double SharedLowFrequencyRoadSignal(double left, double right) {
  return std::abs(left) >= std::abs(right) ? left : right;
}

} // namespace psvr2_toolkit::bridge
