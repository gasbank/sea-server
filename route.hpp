#pragma once

struct xy;

namespace ss {
    class route {
    public:
        typedef std::pair<float, float> fxfy;
        typedef std::pair<fxfy, fxfy> fxfyvxvy;

        route(const std::vector<xy>& waypoints);
        void set_velocity(float v) { velocity = v; }
        void update(float dt);
        fxfyvxvy get_pos() const;
    private:
        std::vector<float> accum_distance;
        std::vector<xy> waypoints;
        float velocity;
        float param;
    };
}
