#if __has_include(<hiredis/hiredis.h>)
#include <hiredis/hiredis.h>
#elif __has_include(<hiredis.h>)
#include <hiredis.h>
#else
#error "hiredis headers not found"
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

struct VehicleState {
    std::string vehicle_id;
    double source_timestamp = 0.0;
    double pos_x = 0.0;
    double pos_y = 0.0;
    double speed = 0.0;
    double acceleration = 0.0;
    double heading = 0.0;
    std::string current_edge_id;
    double lane_pos_m = 0.0;
};

struct PredictedPoint {
    std::string vehicle_id;
    int step_index = 0;
    double predicted_time = 0.0;
    double pos_x = 0.0;
    double pos_y = 0.0;
    double speed = 0.0;
    double heading = 0.0;
    double acceleration = 0.0;
};

struct RouteInputs {
    std::string net_file;
    std::string route_file;
};

static std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos) return "";
    const size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

static std::string env_str(const char* key, const std::string& def) {
    const char* v = std::getenv(key);
    return (v && *v) ? std::string(v) : def;
}

static double env_double(const char* key, double def) {
    const char* v = std::getenv(key);
    if (!v || !*v) return def;
    try {
        return std::stod(v);
    } catch (...) {
        return def;
    }
}

static int env_int(const char* key, int def) {
    const char* v = std::getenv(key);
    if (!v || !*v) return def;
    try {
        return std::stoi(v);
    } catch (...) {
        return def;
    }
}

static std::string fmt6(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.6f", v);
    return std::string(buf);
}

static std::string get_attr(const std::string& line, const std::string& key) {
    const std::string needle = key + "=\"";
    const size_t pos = line.find(needle);
    if (pos == std::string::npos) return "";
    const size_t start = pos + needle.size();
    const size_t end = line.find('"', start);
    if (end == std::string::npos) return "";
    return line.substr(start, end - start);
}

class RedisClient {
public:
    RedisClient(const std::string& host, int port, int db)
        : host_(host), port_(port), db_(db) {}

    ~RedisClient() {
        disconnect();
    }

    bool connect() {
        struct timeval timeout {
            1, 500000
        };
        ctx_ = redisConnectWithTimeout(host_.c_str(), port_, timeout);
        if (!ctx_ || ctx_->err) {
            if (ctx_) {
                std::cerr << "[external_controller_cpp] Redis connect error: " << ctx_->errstr << "\n";
                redisFree(ctx_);
                ctx_ = nullptr;
            }
            return false;
        }
        redisReply* sel = static_cast<redisReply*>(redisCommand(ctx_, "SELECT %d", db_));
        if (!sel || sel->type == REDIS_REPLY_ERROR) {
            std::cerr << "[external_controller_cpp] Redis SELECT failed for db=" << db_ << "\n";
            if (sel) freeReplyObject(sel);
            disconnect();
            return false;
        }
        freeReplyObject(sel);
        return true;
    }

    void disconnect() {
        if (ctx_) {
            redisFree(ctx_);
            ctx_ = nullptr;
        }
    }

    bool connected() const { return ctx_ != nullptr; }

    std::vector<std::string> keys(const std::string& pattern) {
        std::vector<std::string> out;
        redisReply* r = static_cast<redisReply*>(redisCommand(ctx_, "KEYS %s", pattern.c_str()));
        if (!r) return out;
        if (r->type == REDIS_REPLY_ARRAY) {
            for (size_t i = 0; i < r->elements; ++i) {
                if (r->element[i] && r->element[i]->str) out.emplace_back(r->element[i]->str);
            }
        }
        freeReplyObject(r);
        return out;
    }

    std::unordered_map<std::string, std::string> hgetall(const std::string& key) {
        std::unordered_map<std::string, std::string> out;
        redisReply* r = static_cast<redisReply*>(redisCommand(ctx_, "HGETALL %s", key.c_str()));
        if (!r) return out;
        if (r->type == REDIS_REPLY_ARRAY) {
            for (size_t i = 0; i + 1 < r->elements; i += 2) {
                const char* k = r->element[i] && r->element[i]->str ? r->element[i]->str : "";
                const char* v = r->element[i + 1] && r->element[i + 1]->str ? r->element[i + 1]->str : "";
                out[k] = v;
            }
        }
        freeReplyObject(r);
        return out;
    }

    void hset_latest(const std::string& key,
                     uint64_t cycle_id,
                     double generated_at,
                     double horizon_s,
                     double step_s,
                     int trajectory_count,
                     int ttl_s) {
        redisReply* r = static_cast<redisReply*>(redisCommand(
            ctx_,
            "HSET %s cycle_id %llu generated_at %s horizon_s %s step_s %s trajectory_count %d",
            key.c_str(),
            static_cast<unsigned long long>(cycle_id),
            fmt6(generated_at).c_str(),
            fmt6(horizon_s).c_str(),
            fmt6(step_s).c_str(),
            trajectory_count));
        if (r) freeReplyObject(r);
        (void)ttl_s;
    }

    void xadd_prediction(const std::string& stream_key,
                         int maxlen,
                         uint64_t cycle_id,
                         const PredictedPoint& p) {
        redisReply* r = static_cast<redisReply*>(redisCommand(
            ctx_,
            "XADD %s MAXLEN ~ %d * "
            "cycle_id %llu "
            "vehicle_id %s "
            "step_index %d "
            "predicted_time %s "
            "pos_x %s "
            "pos_y %s "
            "speed %s "
            "heading %s "
            "acceleration %s",
            stream_key.c_str(),
            maxlen,
            static_cast<unsigned long long>(cycle_id),
            p.vehicle_id.c_str(),
            p.step_index,
            fmt6(p.predicted_time).c_str(),
            fmt6(p.pos_x).c_str(),
            fmt6(p.pos_y).c_str(),
            fmt6(p.speed).c_str(),
            fmt6(p.heading).c_str(),
            fmt6(p.acceleration).c_str()));
        if (r) freeReplyObject(r);
    }

private:
    std::string host_;
    int port_ = 6379;
    int db_ = 0;
    redisContext* ctx_ = nullptr;
};

class ExternalControllerCpp {
public:
    ExternalControllerCpp()
        : run_id_(env_str("DT2_RUN_ID", "DT-Secondary-MotionChannel")),
          project_dir_("."),
          prediction_horizon_s_(env_double("DT2_PRED_HORIZON_S", 0.5)),
          prediction_step_s_(env_double("DT2_PRED_STEP_S", 0.02)),
          prediction_ttl_s_(env_int("DT2_PRED_TTL_S", 2)),
          poll_interval_s_(env_double("DT2_POLL_INTERVAL_S", 0.1)),
          pred_stream_maxlen_(env_int("DT2_PRED_STREAM_MAXLEN", 200000)),
          redis_(env_str("REDIS_HOST", "127.0.0.1"), env_int("REDIS_PORT", 6379), env_int("REDIS_DB", 0)) {}

    bool init(const std::string& project_dir) {
        project_dir_ = project_dir;
        if (!redis_.connect()) {
            return false;
        }
        load_static_route_knowledge();
        std::cout << "[external_controller_cpp] started run_id=" << run_id_ << " poll=" << poll_interval_s_ << "s\n";
        return true;
    }

    void run() {
        while (true) {
            try {
                maybe_publish_prediction_cycle();
            } catch (const std::exception& e) {
                std::cerr << "[external_controller_cpp] loop error: " << e.what() << "\n";
            }
            sleep_for(poll_interval_s_);
        }
    }

private:
    void sleep_for(double seconds) {
        const auto ms = std::chrono::milliseconds(static_cast<int>(std::max(1.0, seconds * 1000.0)));
        std::this_thread::sleep_for(ms);
    }

    double to_double(const std::unordered_map<std::string, std::string>& m,
                     const std::string& key,
                     double def = 0.0) const {
        auto it = m.find(key);
        if (it == m.end()) return def;
        try {
            return std::stod(it->second);
        } catch (...) {
            return def;
        }
    }

    RouteInputs resolve_sumo_inputs() const {
        const std::string ini_path = project_dir_ + "/omnetpp.ini";
        std::ifstream in(ini_path);
        std::string manager_cfg = "erlangen.sumo.cfg";
        std::string line;
        while (std::getline(in, line)) {
            const std::string t = trim(line);
            if (t.rfind("*.manager.configFile", 0) == 0) {
                const size_t eq = t.find('=');
                if (eq != std::string::npos) {
                    std::string rhs = trim(t.substr(eq + 1));
                    if (!rhs.empty() && rhs.front() == '"' && rhs.back() == '"' && rhs.size() >= 2) {
                        rhs = rhs.substr(1, rhs.size() - 2);
                    }
                    if (!rhs.empty()) manager_cfg = rhs;
                }
                break;
            }
        }

        RouteInputs out;
        const std::string cfg_path = project_dir_ + "/" + manager_cfg;
        std::ifstream cfg(cfg_path);
        std::string text;
        while (std::getline(cfg, line)) {
            text += line;
            text.push_back('\n');
        }
        std::smatch m;
        std::regex net_re(R"(<net-file[^>]*value=\"([^\"]+)\")");
        std::regex route_re(R"(<route-files[^>]*value=\"([^\"]+)\")");
        if (std::regex_search(text, m, net_re) && m.size() > 1) {
            out.net_file = project_dir_ + "/" + m[1].str();
        }
        if (std::regex_search(text, m, route_re) && m.size() > 1) {
            std::string routes = m[1].str();
            const size_t comma = routes.find(',');
            if (comma != std::string::npos) routes = routes.substr(0, comma);
            out.route_file = project_dir_ + "/" + trim(routes);
        }
        return out;
    }

    std::vector<std::pair<double, double>> parse_shape(const std::string& shape) const {
        std::vector<std::pair<double, double>> pts;
        std::istringstream ss(shape);
        std::string tok;
        while (ss >> tok) {
            const size_t c = tok.find(',');
            if (c == std::string::npos) continue;
            try {
                const double x = std::stod(tok.substr(0, c));
                const double y = std::stod(tok.substr(c + 1));
                pts.emplace_back(x, y);
            } catch (...) {
            }
        }
        return pts;
    }

    double polyline_length(const std::vector<std::pair<double, double>>& pts) const {
        double t = 0.0;
        for (size_t i = 1; i < pts.size(); ++i) {
            const double dx = pts[i].first - pts[i - 1].first;
            const double dy = pts[i].second - pts[i - 1].second;
            t += std::hypot(dx, dy);
        }
        return t;
    }

    std::tuple<double, double, double> interpolate_on_shape(const std::vector<std::pair<double, double>>& pts,
                                                             double offset_m) const {
        if (pts.empty()) return {0.0, 0.0, 0.0};
        if (pts.size() == 1) return {pts[0].first, pts[0].second, 0.0};

        double travelled = 0.0;
        for (size_t i = 1; i < pts.size(); ++i) {
            const double x0 = pts[i - 1].first;
            const double y0 = pts[i - 1].second;
            const double x1 = pts[i].first;
            const double y1 = pts[i].second;
            const double seg = std::hypot(x1 - x0, y1 - y0);
            if (offset_m <= travelled + seg || i == pts.size() - 1) {
                double rel = (seg <= 1e-9) ? 0.0 : (offset_m - travelled) / seg;
                rel = std::max(0.0, std::min(1.0, rel));
                const double x = x0 + rel * (x1 - x0);
                const double y = y0 + rel * (y1 - y0);
                const double heading = std::atan2(y1 - y0, x1 - x0) * 180.0 / M_PI;
                return {x, y, heading};
            }
            travelled += seg;
        }
        const double x0 = pts[pts.size() - 2].first;
        const double y0 = pts[pts.size() - 2].second;
        const double x1 = pts.back().first;
        const double y1 = pts.back().second;
        return {x1, y1, std::atan2(y1 - y0, x1 - x0) * 180.0 / M_PI};
    }

    int resolve_route_edge_index(const std::string& vehicle_id,
                                 const std::string& current_edge,
                                 const std::vector<std::string>& edges) {
        if (!current_edge.empty()) {
            for (size_t i = 0; i < edges.size(); ++i) {
                if (edges[i] == current_edge) {
                    vehicle_route_cursor_[vehicle_id] = static_cast<int>(i);
                    return static_cast<int>(i);
                }
            }
        }
        auto it = vehicle_route_cursor_.find(vehicle_id);
        if (it != vehicle_route_cursor_.end() && it->second >= 0 && it->second < static_cast<int>(edges.size())) {
            return it->second;
        }
        return -1;
    }

    std::vector<PredictedPoint> predict_constant_velocity(const VehicleState& v) const {
        std::vector<PredictedPoint> out;
        const int steps = std::max(1, static_cast<int>(std::floor(prediction_horizon_s_ / prediction_step_s_ + 1e-9)));
        const double heading_rad = v.heading * M_PI / 180.0;
        const double cos_h = std::cos(heading_rad);
        const double sin_h = std::sin(heading_rad);
        for (int i = 1; i <= steps; ++i) {
            const double t = i * prediction_step_s_;
            const double delta = std::max(0.0, v.speed * t + 0.5 * v.acceleration * t * t);
            PredictedPoint p;
            p.vehicle_id = v.vehicle_id;
            p.step_index = i;
            p.predicted_time = v.source_timestamp + t;
            p.pos_x = v.pos_x + delta * cos_h;
            p.pos_y = v.pos_y + delta * sin_h;
            p.speed = std::max(0.0, v.speed + v.acceleration * t);
            p.heading = v.heading;
            p.acceleration = v.acceleration;
            out.push_back(p);
        }
        return out;
    }

    std::vector<PredictedPoint> predict_vehicle(const VehicleState& v) {
        auto rt = vehicle_routes_.find(v.vehicle_id);
        if (rt == vehicle_routes_.end()) return predict_constant_velocity(v);
        const auto& route_edges = rt->second;

        int edge_index = resolve_route_edge_index(v.vehicle_id, v.current_edge_id, route_edges);
        if (edge_index < 0) return predict_constant_velocity(v);

        std::vector<PredictedPoint> out;
        const int steps = std::max(1, static_cast<int>(std::floor(prediction_horizon_s_ / prediction_step_s_ + 1e-9)));
        double lane_pos = std::max(0.0, v.lane_pos_m);

        if (!v.current_edge_id.empty() && v.current_edge_id[0] == ':') {
            auto it_len = edge_lengths_.find(route_edges[edge_index]);
            if (it_len != edge_lengths_.end()) lane_pos = it_len->second;
        }

        for (int step_idx = 1; step_idx <= steps; ++step_idx) {
            const double t = step_idx * prediction_step_s_;
            const double delta_s = std::max(0.0, v.speed * t + 0.5 * v.acceleration * t * t);
            const double projected_speed = std::max(0.0, v.speed + v.acceleration * t);

            int cursor = edge_index;
            double offset = lane_pos + delta_s;
            while (cursor < static_cast<int>(route_edges.size())) {
                const std::string& edge_id = route_edges[cursor];
                const double edge_len = edge_lengths_.count(edge_id) ? edge_lengths_[edge_id] : 0.0;
                if (offset <= edge_len || cursor == static_cast<int>(route_edges.size()) - 1) {
                    auto shape_it = edge_shapes_.find(edge_id);
                    if (shape_it == edge_shapes_.end()) {
                        break;
                    }
                    auto [x, y, heading] = interpolate_on_shape(shape_it->second, std::min(offset, edge_len));
                    PredictedPoint p;
                    p.vehicle_id = v.vehicle_id;
                    p.step_index = step_idx;
                    p.predicted_time = v.source_timestamp + t;
                    p.pos_x = x;
                    p.pos_y = y;
                    p.speed = projected_speed;
                    p.heading = heading;
                    p.acceleration = v.acceleration;
                    out.push_back(p);
                    break;
                }
                offset -= edge_len;
                cursor += 1;
            }
        }

        if (out.empty()) return predict_constant_velocity(v);
        return out;
    }

    void load_static_route_knowledge() {
        const RouteInputs inputs = resolve_sumo_inputs();
        if (inputs.net_file.empty() || inputs.route_file.empty()) {
            std::cerr << "[external_controller_cpp] Could not resolve SUMO inputs, fallback to constant-velocity only\n";
            return;
        }

        {
            std::ifstream in(inputs.net_file);
            std::string line;
            std::string current_edge;
            bool current_internal = false;
            while (std::getline(in, line)) {
                const std::string t = trim(line);
                if (t.find("<edge ") != std::string::npos) {
                    current_edge = get_attr(t, "id");
                    current_internal = (get_attr(t, "function") == "internal");
                    if (!current_edge.empty() && !current_internal && t.find("<lane ") != std::string::npos) {
                        const std::string shape = get_attr(t, "shape");
                        auto pts = parse_shape(shape);
                        if (!pts.empty()) {
                            edge_shapes_[current_edge] = pts;
                            edge_lengths_[current_edge] = polyline_length(pts);
                        }
                    }
                    if (t.find("/>") != std::string::npos) {
                        current_edge.clear();
                        current_internal = false;
                    }
                    continue;
                }
                if (!current_edge.empty() && !current_internal && t.find("<lane ") != std::string::npos) {
                    const std::string shape = get_attr(t, "shape");
                    auto pts = parse_shape(shape);
                    if (!pts.empty()) {
                        edge_shapes_[current_edge] = pts;
                        edge_lengths_[current_edge] = polyline_length(pts);
                    }
                }
                if (t.find("</edge>") != std::string::npos) {
                    current_edge.clear();
                    current_internal = false;
                }
            }
        }

        std::unordered_map<std::string, std::vector<std::string>> route_defs;
        {
            std::ifstream in(inputs.route_file);
            std::string line;
            while (std::getline(in, line)) {
                const std::string t = trim(line);
                if (t.find("<route ") != std::string::npos) {
                    const std::string route_id = get_attr(t, "id");
                    const std::string edges = get_attr(t, "edges");
                    if (!route_id.empty() && !edges.empty()) {
                        std::vector<std::string> e;
                        std::istringstream ss(edges);
                        std::string tok;
                        while (ss >> tok) e.push_back(tok);
                        route_defs[route_id] = e;
                    }
                }
                if (t.find("<vehicle ") != std::string::npos) {
                    const std::string vehicle_id = get_attr(t, "id");
                    const std::string route_id = get_attr(t, "route");
                    if (!vehicle_id.empty() && !route_id.empty() && route_defs.count(route_id)) {
                        vehicle_routes_[vehicle_id] = route_defs[route_id];
                        if (vehicle_id.size() > 1 && vehicle_id[0] == 'v') {
                            bool numeric = true;
                            for (size_t i = 1; i < vehicle_id.size(); ++i) {
                                if (vehicle_id[i] < '0' || vehicle_id[i] > '9') {
                                    numeric = false;
                                    break;
                                }
                            }
                            if (numeric) {
                                vehicle_routes_[vehicle_id.substr(1)] = route_defs[route_id];
                            }
                        }
                    }
                }
            }
        }

        std::cout << "[external_controller_cpp] loaded routes=" << vehicle_routes_.size()
                  << " edges=" << edge_shapes_.size() << "\n";
    }

    std::vector<VehicleState> get_all_vehicle_states() {
        std::vector<VehicleState> out;

        const std::string dt2_pattern = "dt2:vehicle:" + run_id_ + ":*:latest";
        auto dt2_keys = redis_.keys(dt2_pattern);
        for (const auto& key : dt2_keys) {
            const auto state = redis_.hgetall(key);
            if (state.empty()) continue;

            const std::vector<std::string> parts = split(key, ':');
            if (parts.size() < 5) continue;
            const std::string vehicle_id = parts[parts.size() - 2];

            VehicleState v;
            v.vehicle_id = vehicle_id;
            v.source_timestamp = to_double(state, "sim_time", 0.0);
            v.pos_x = to_double(state, "pos_x", 0.0);
            v.pos_y = to_double(state, "pos_y", 0.0);
            v.speed = to_double(state, "speed", 0.0);
            v.acceleration = to_double(state, "acceleration", 0.0);
            v.heading = to_double(state, "heading", 0.0);

            const auto route = redis_.hgetall("vehicle:" + vehicle_id + ":route_progress");
            if (!route.empty()) {
                auto it = route.find("current_edge_id");
                if (it != route.end()) v.current_edge_id = it->second;
                v.lane_pos_m = to_double(route, "lane_pos_m", 0.0);
                const double route_ts = to_double(route, "source_timestamp", v.source_timestamp);
                if (v.source_timestamp <= 0.0) v.source_timestamp = route_ts;
            }
            out.push_back(v);
        }

        if (!out.empty()) return out;

        auto keys = redis_.keys("vehicle:*:state");
        for (const auto& key : keys) {
            const auto state = redis_.hgetall(key);
            if (state.empty()) continue;
            const auto parts = split(key, ':');
            if (parts.size() < 3) continue;
            const std::string vehicle_id = parts[1];

            VehicleState v;
            v.vehicle_id = vehicle_id;
            v.source_timestamp = to_double(state, "source_timestamp", 0.0);
            v.pos_x = to_double(state, "pos_x", 0.0);
            v.pos_y = to_double(state, "pos_y", 0.0);
            v.speed = to_double(state, "speed", 0.0);
            v.acceleration = to_double(state, "acceleration", 0.0);
            v.heading = to_double(state, "heading", 0.0);

            const auto route = redis_.hgetall("vehicle:" + vehicle_id + ":route_progress");
            if (!route.empty()) {
                auto it = route.find("current_edge_id");
                if (it != route.end()) v.current_edge_id = it->second;
                v.lane_pos_m = to_double(route, "lane_pos_m", 0.0);
                const double route_ts = to_double(route, "source_timestamp", v.source_timestamp);
                if (v.source_timestamp <= 0.0) v.source_timestamp = route_ts;
            }
            out.push_back(v);
        }
        return out;
    }

    static std::vector<std::string> split(const std::string& s, char d) {
        std::vector<std::string> out;
        std::string cur;
        for (char c : s) {
            if (c == d) {
                out.push_back(cur);
                cur.clear();
            } else {
                cur.push_back(c);
            }
        }
        out.push_back(cur);
        return out;
    }

    void maybe_publish_prediction_cycle() {
        auto vehicles = get_all_vehicle_states();
        if (vehicles.empty()) return;

        double latest_source_ts = 0.0;
        for (const auto& v : vehicles) latest_source_ts = std::max(latest_source_ts, v.source_timestamp);
        if (latest_source_ts <= last_prediction_source_ts_) return;

        prediction_cycle_id_ += 1;
        const uint64_t cycle_id = prediction_cycle_id_;
        const std::string stream_key = "dt2:pred:" + run_id_ + ":cycle:" + std::to_string(cycle_id) + ":entries";
        const std::string latest_key = "dt2:pred:" + run_id_ + ":latest";

        int trajectory_count = 0;
        for (const auto& v : vehicles) {
            auto pred = predict_vehicle(v);
            if (pred.empty()) continue;
            trajectory_count += 1;
            std::cout << "[external_controller_cpp] vehicle=" << v.vehicle_id
                      << " predicted_points=" << pred.size()
                      << " horizon_s=" << prediction_horizon_s_
                      << " step_s=" << prediction_step_s_ << "\n";
            for (const auto& p : pred) {
                redis_.xadd_prediction(stream_key, pred_stream_maxlen_, cycle_id, p);
            }
        }

        if (trajectory_count > 0) {
            redis_.hset_latest(
                latest_key,
                cycle_id,
                latest_source_ts,
                prediction_horizon_s_,
                prediction_step_s_,
                trajectory_count,
                prediction_ttl_s_);

            last_prediction_source_ts_ = latest_source_ts;
            std::cout << "[external_controller_cpp] prediction cycle=" << cycle_id
                      << " trajectories=" << trajectory_count
                      << " source_ts=" << latest_source_ts << "\n";
        }
    }

private:
    std::string run_id_;
    std::string project_dir_;
    double prediction_horizon_s_ = 0.5;
    double prediction_step_s_ = 0.02;
    int prediction_ttl_s_ = 2;
    double poll_interval_s_ = 0.1;
    int pred_stream_maxlen_ = 200000;

    RedisClient redis_;

    std::unordered_map<std::string, std::vector<std::string>> vehicle_routes_;
    std::unordered_map<std::string, std::vector<std::pair<double, double>>> edge_shapes_;
    std::unordered_map<std::string, double> edge_lengths_;
    std::unordered_map<std::string, int> vehicle_route_cursor_;

    double last_prediction_source_ts_ = -1.0;
    uint64_t prediction_cycle_id_ = 0;
};

}  // namespace

int main(int argc, char** argv) {
    std::string project_dir = ".";
    if (argc > 1 && argv[1]) {
        project_dir = argv[1];
    }

    ExternalControllerCpp controller;
    if (!controller.init(project_dir)) {
        std::cerr << "[external_controller_cpp] initialization failed\n";
        return 1;
    }
    controller.run();
    return 0;
}
