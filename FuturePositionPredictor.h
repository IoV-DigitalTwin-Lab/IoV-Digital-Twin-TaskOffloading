#ifndef __FUTURE_POSITION_PREDICTOR_H_
#define __FUTURE_POSITION_PREDICTOR_H_

#include <memory>
#include <string>
#include <vector>

namespace complex_network {

struct PredictorInputState {
    std::string vehicle_id;
    double source_timestamp = 0.0;
    double pos_x = 0.0;
    double pos_y = 0.0;
    double speed = 0.0;
    double heading_deg = 0.0;
    double acceleration = 0.0;
};

struct PredictedVehicleState {
    std::string vehicle_id;
    int step_index = 0;
    double sim_time = 0.0;
    double pos_x = 0.0;
    double pos_y = 0.0;
    double speed = 0.0;
    double heading_deg = 0.0;
    double acceleration = 0.0;
};

class IFuturePositionPredictor {
public:
    virtual ~IFuturePositionPredictor() = default;

    virtual std::string mode() const = 0;
    virtual std::vector<PredictedVehicleState> predict(const PredictorInputState& input,
                                                       double horizon_s,
                                                       double step_s) const = 0;
};

class PlaceholderConstantVelocityPredictor : public IFuturePositionPredictor {
public:
    std::string mode() const override;
    std::vector<PredictedVehicleState> predict(const PredictorInputState& input,
                                               double horizon_s,
                                               double step_s) const override;
};

std::unique_ptr<IFuturePositionPredictor> createFuturePositionPredictor(const std::string& mode);

} // namespace complex_network

#endif
