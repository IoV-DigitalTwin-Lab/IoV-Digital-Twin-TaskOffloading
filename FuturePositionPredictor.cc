#include "FuturePositionPredictor.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace complex_network {

namespace {

constexpr double kPi = 3.14159265358979323846;

std::string normalizeMode(const std::string& mode) {
    std::string out = mode;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

} // namespace

std::string PlaceholderConstantVelocityPredictor::mode() const {
    return "placeholder";
}

std::vector<PredictedVehicleState> PlaceholderConstantVelocityPredictor::predict(const PredictorInputState& input,
                                                                                 double horizon_s,
                                                                                 double step_s) const {
    std::vector<PredictedVehicleState> states;
    if (horizon_s <= 0.0 || step_s <= 0.0) {
        return states;
    }

    const int steps = std::max(1, static_cast<int>(std::floor(horizon_s / step_s + 1e-9)));
    const double heading_rad = input.heading_deg * (kPi / 180.0);
    const double cos_h = std::cos(heading_rad);
    const double sin_h = std::sin(heading_rad);

    for (int k = 1; k <= steps; ++k) {
        const double t = k * step_s;
        const double delta = input.speed * t + 0.5 * input.acceleration * t * t;

        PredictedVehicleState st;
        st.vehicle_id = input.vehicle_id;
        st.step_index = k;
        st.sim_time = input.source_timestamp + t;
        st.pos_x = input.pos_x + delta * cos_h;
        st.pos_y = input.pos_y + delta * sin_h;
        st.speed = std::max(0.0, input.speed + input.acceleration * t);
        st.heading_deg = input.heading_deg;
        st.acceleration = input.acceleration;
        states.push_back(st);
    }

    return states;
}

std::unique_ptr<IFuturePositionPredictor> createFuturePositionPredictor(const std::string& mode) {
    const std::string normalized = normalizeMode(mode);
    if (normalized.empty() || normalized == "placeholder" || normalized == "constant_velocity") {
        return std::make_unique<PlaceholderConstantVelocityPredictor>();
    }

    // Unknown mode falls back to placeholder so simulation can continue.
    return std::make_unique<PlaceholderConstantVelocityPredictor>();
}

} // namespace complex_network
