#include "LinkRateModel.h"

#include <algorithm>
#include <cmath>

namespace complex_network {

double LinkRateModel::effectiveRateBps(double sinr_db,
                                       double bandwidth_hz,
                                       double efficiency,
                                       double cap_bps) {
    const double safe_bw = std::max(1.0, bandwidth_hz);
    const double safe_eff = std::max(0.01, std::min(1.0, efficiency));
    const double sinr_linear = std::pow(10.0, sinr_db / 10.0);
    double rate = safe_eff * safe_bw * std::log2(1.0 + std::max(0.0, sinr_linear));

    if (cap_bps > 0.0) {
        rate = std::min(rate, cap_bps);
    }
    return std::max(1.0, rate);
}

double LinkRateModel::transferDelaySeconds(double payload_bytes, double rate_bps) {
    const double bits = std::max(0.0, payload_bytes) * 8.0;
    const double safe_rate = std::max(1.0, rate_bps);
    return bits / safe_rate;
}

} // namespace complex_network
