#ifndef __LINK_RATE_MODEL_H_
#define __LINK_RATE_MODEL_H_

namespace complex_network {

class LinkRateModel {
public:
    // Returns effective throughput in bits/s using Shannon-style mapping.
    static double effectiveRateBps(double sinr_db,
                                   double bandwidth_hz,
                                   double efficiency,
                                   double cap_bps = -1.0);

    // Returns transmission time in seconds for a payload over given rate.
    static double transferDelaySeconds(double payload_bytes, double rate_bps);
};

} // namespace complex_network

#endif
