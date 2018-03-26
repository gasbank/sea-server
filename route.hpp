#pragma once

struct xy;

namespace ss {
    class route {
    public:
        route(const std::vector<xy>& waypoints);
        void set_velocity(float v) { velocity = v; }
        void update(float dt);
        std::pair<float, float> get_pos() const;
    private:
        std::vector<float> accum_distance;
        std::vector<xy> waypoints;
        float velocity;
        float param;
    };
}
