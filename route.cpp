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

route::fxfyvxvy route::get_pos(bool& finished) const {
    auto it = std::lower_bound(accum_distance.begin(), accum_distance.end(), param);
    auto px = 0.0f, py = 0.0f, dx = 0.0f, dy = 0.0f;
    finished = false;
    if (it == accum_distance.begin()) {
        px = static_cast<float>(waypoints.begin()->x);
        py = static_cast<float>(waypoints.begin()->y);
        dx = static_cast<float>((waypoints.begin() + 1)->x) - px;
        dy = static_cast<float>((waypoints.begin() + 1)->y) - py;
    } else if (it == accum_distance.end()) {
        px = static_cast<float>(waypoints.rbegin()->x);
        py = static_cast<float>(waypoints.rbegin()->y);
        dx = 0;
        dy = 0;
        finished = true;
    } else {
        auto it_idx = it - accum_distance.begin();
        auto wp1 = waypoints[it_idx - 1];
        auto wp2 = waypoints[it_idx];
        auto d1 = accum_distance[it_idx - 1];
        auto d2 = accum_distance[it_idx];
        auto r = (param - d1) / (d2 - d1);
        if (r < 0) r = 0;
        if (r > 1) r = 1;
        dx = static_cast<float>(wp2.x - wp1.x);
        dy = static_cast<float>(wp2.y - wp1.y);
        px = wp1.x + dx * r;
        py = wp1.y + dy * r;
    }
    return std::make_pair(std::make_pair(px, py), std::make_pair(dx, dy));
}

float route::get_left() const {
    return std::max(0.0f, *accum_distance.rbegin() - param);
}

void route::reverse() {
    std::reverse(waypoints.begin(), waypoints.end());
    auto total_length = *accum_distance.rbegin();
    for (size_t i = 0; i < accum_distance.size(); i++) {
        accum_distance[i] = total_length - accum_distance[i];
    }
    std::reverse(accum_distance.begin(), accum_distance.end());
    param = total_length - param;
}