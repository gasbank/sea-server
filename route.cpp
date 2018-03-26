#include "precompiled.hpp"
#include "route.hpp"
#include "xy.hpp"

using namespace ss;

float distance_xy(const xy& a, const xy& b) {
    return sqrtf(static_cast<float>((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y)));
}

route::route(const std::vector<xy>& waypoints)
    : waypoints(waypoints)
    , velocity(0)
    , param(0) {
    if (waypoints.size() == 0) {
        std::cerr << "route created with empty waypoints..." << std::endl;
    } else {
        float dist = 0;
        accum_distance.push_back(dist);
        for (size_t i = 0; i < waypoints.size() - 1; i++) {
            dist += distance_xy(waypoints[i], waypoints[i+1]);
            accum_distance.push_back(dist);
        }
    }
}

void route::update(float dt) {
    param += velocity * dt;
}

std::pair<float, float> route::get_pos() const {
    auto it = std::lower_bound(accum_distance.begin(), accum_distance.end(), param);
    if (it == accum_distance.begin()) {
        return std::make_pair(static_cast<float>(waypoints.begin()->x), static_cast<float>(waypoints.begin()->y));
    } else if (it == accum_distance.end()) {
        return std::make_pair(static_cast<float>(waypoints.rbegin()->x), static_cast<float>(waypoints.rbegin()->y));
    } else {
        auto it_idx = it - accum_distance.begin();
        auto wp1 = waypoints[it_idx - 1];
        auto wp2 = waypoints[it_idx];
        auto d1 = accum_distance[it_idx - 1];
        auto d2 = accum_distance[it_idx];
        auto r = (param - d1) / (d2 - d1);
        if (r < 0) r = 0;
        if (r > 1) r = 1;
        return std::make_pair(wp1.x + (wp2.x - wp1.x) * r, wp1.y + (wp2.y - wp1.y) * r);
    }
}
