#include "precompiled.hpp"
#include "sea.hpp"
#include "route.hpp"
using namespace ss;

sea::sea()
    : res_width(WORLD_MAP_PIXEL_RESOLUTION_WIDTH)
    , res_height(WORLD_MAP_PIXEL_RESOLUTION_HEIGHT)
    , km_per_cell(WORLD_CIRCUMFERENCE_IN_KM / res_width) {
}

float sea::lng_to_xc(float lng) const {
    return res_width / 2.0f + lng / 180.0f * res_width / 2.0f;
}

float sea::lat_to_yc(float lat) const {
    return res_height / 2.0f - lat / 90.0f * res_height / 2.0f;
}

void sea::populate_test() {
    boost::random::mt19937 rng;
    boost::random::uniform_real_distribution<float> random_point(-2500, 2500);
    for (int i = 0; i < 0; i++) {
        float x = random_point(rng);
        float y = random_point(rng);
        spawn(i + 1, x, y, 1, 1);
        if ((i + 1) % 5000 == 0) {
            std::cout << "Spawning " << (i + 1) << "..." << std::endl;
        }
    }
    //travel_to("Test A", 129.393311f + 1.0f, 35.4739418f, 0.01f);
    std::cout << "Spawning finished." << std::endl;
    std::vector<sea_object_public> sop_list;
    query_near_to_packet(0, 0, 10, sop_list);
}

int sea::spawn(const char* guid, int type, float x, float y, float w, float h) {
    auto it = sea_guid_to_id.find(guid);
    if (it != sea_guid_to_id.end()) {
        int id = it->second;
        auto it2 = sea_objects.find(id);
        if (it2 != sea_objects.end()) {
            return id;
        } else {
            std::cerr << boost::format("Spawn: Orphan entry found in sea_guid_to_id. (guid=%1%) Removing and respawn...") % guid;
            sea_guid_to_id.erase(it);
            return spawn(guid, type, x, y, w, h);
        }
    } else {
        int id = spawn(type, x, y, w, h);
        sea_objects.find(id)->second.set_guid(guid);
        sea_guid_to_id[guid] = id;
        return id;
    }
}

int sea::spawn(int type, float x, float y, float w, float h) {
    int id = static_cast<int>(sea_objects.size()) + 1;
    box b(point(x, y), point(x + w, y + h));
    auto rtree_value = std::make_pair(b, id);
    sea_objects.emplace(std::pair<int, sea_object>(id, sea_object(id, type, x, y, w, h, rtree_value)));
    rtree.insert(rtree_value);
    return id;
}

void sea::travel_to(const char* guid, float x, float y, float v) {
    auto it = sea_guid_to_id.find(guid);
    if (it != sea_guid_to_id.end()) {
        auto it2 = sea_objects.find(it->second);
        if (it2 != sea_objects.end()) {
            rtree.remove(it2->second.get_rtree_value());
            float cx = 0, cy = 0; // current position
            it2->second.get_xy(cx, cy);
            float dx = x - cx;
            float dy = y - cy;
            float dlen = sqrtf(dx * dx + dy * dy);
            if (dlen > 1e-3) {
                it2->second.set_velocity(dx / dlen * v, dy / dlen * v);
            } else {
                it2->second.set_velocity(0, 0);
            }
            it2->second.set_destination(x, y);
            rtree.insert(it2->second.get_rtree_value());
        } else {
            std::cerr << boost::format("Sea object not found corresponding to guid %1%\n") % guid;
        }
    } else {
        std::cerr << boost::format("Sea object ID not found corresponding to guid %1%\n") % guid;
    }
}

SEA_OBJECT_STATE sea::get_object_state(int id) const {
    auto it = sea_objects.find(id);
    if (it != sea_objects.end()) {
        return it->second.get_state();
    } else {
        std::cerr << boost::format("Sea object not found corresponding to id %1%\n") % id;
    }
    return SOS_ERROR;
}

void sea::set_object_state(int id, SEA_OBJECT_STATE state) {
    auto it = sea_objects.find(id);
    if (it != sea_objects.end()) {
        it->second.set_state(state);
    } else {
        std::cerr << boost::format("Sea object not found corresponding to id %1%\n") % id;
    }
}

sea_object* sea::get_object(int id) {
    auto it = sea_objects.find(id);
    if (it != sea_objects.end()) {
        return &it->second;
    } else {
        std::cerr << boost::format("Sea object not found corresponding to id %1%\n") % id;
    }
    return nullptr;
}

void sea::teleport_to(int id, float x, float y, float vx, float vy) {
    auto it = sea_objects.find(id);
    if (it != sea_objects.end()) {
        rtree.remove(it->second.get_rtree_value());
        it->second.set_xy(x, y);
        it->second.set_velocity(vx, vy);
        rtree.insert(it->second.get_rtree_value());
    } else {
        std::cerr << boost::format("Sea object not found corresponding to id %1%\n") % id;
    }
}

void sea::teleport_to(const char* guid, float x, float y, float vx, float vy) {
    auto it = sea_guid_to_id.find(guid);
    if (it != sea_guid_to_id.end()) {
        teleport_to(it->second, x, y, vx, vy);
    } else {
        std::cerr << boost::format("Sea object ID not found corresponding to guid %1%\n") % guid;
    }
}

void sea::teleport_by(const char* guid, float dx, float dy) {
    auto it = sea_guid_to_id.find(guid);
    if (it != sea_guid_to_id.end()) {
        auto it2 = sea_objects.find(it->second);
        if (it2 != sea_objects.end()) {
            rtree.remove(it2->second.get_rtree_value());
            it2->second.translate_xy(dx, dy);
            rtree.insert(it2->second.get_rtree_value());
        } else {
            std::cerr << boost::format("Sea object not found corresponding to guid %1%\n") % guid;
        }
    } else {
        std::cerr << boost::format("Sea object ID not found corresponding to guid %1%\n") % guid;
    }
}

void sea::query_near_lng_lat_to_packet(float lng, float lat, int halfex, std::vector<sea_object_public>& sop_list) const {
    query_near_to_packet(lng_to_xc(lng), lat_to_yc(lat), halfex * 2.0f, sop_list);
}

void sea::query_near_to_packet(float xc, float yc, float ex, std::vector<sea_object_public>& sop_list) const {
    auto id_list = query_tree(xc, yc, ex);
    sop_list.resize(id_list.size());
    std::size_t i = 0;
    BOOST_FOREACH(int id, id_list) {
        auto f = sea_objects.find(id);
        if (f != sea_objects.end()) {
            f->second.fill_sop(sop_list[i]);
            i++;
        }
    }
}

std::vector<int> sea::query_tree(float xc, float yc, float ex) const {
    box query_box(point(xc - ex / 2, yc - ex / 2), point(xc + ex / 2, yc + ex / 2));
    std::vector<value> result_s;
    std::vector<int> id_list;
    rtree.query(bgi::intersects(query_box), std::back_inserter(result_s));
    BOOST_FOREACH(value const& v, result_s) {
        id_list.push_back(v.second);
    }
    return id_list;
}

void sea::update(float delta_time) {
    BOOST_FOREACH(const auto& it, sea_guid_to_id) {
        const auto& it2 = sea_objects.find(it.second);
        if (it2 != sea_objects.cend()) {
            float vx = 0, vy = 0;
            it2->second.get_velocity(vx, vy);
            if (vx || vy) {
                if (it2->second.get_distance_to_destination() > 0.001f) {
                    teleport_by(it.first.c_str(), vx * delta_time, vy * delta_time);
                } else {
                    it2->second.set_velocity(0, 0);
                }
            }
        }
    }
    for (auto& obj : sea_objects) {
        obj.second.update(delta_time);
    }
}

void sea::update_route(float delta_time, int id, std::shared_ptr<route> r) {
    auto finished = false;
    auto pos = r->get_pos(finished);
    auto state = get_object_state(id);
    if (state == SOS_SAILING) {
        r->update(delta_time);
        auto dlen = sqrtf(pos.second.first * pos.second.first + pos.second.second * pos.second.second);
        if (dlen) {
            teleport_to(id, pos.first.first, pos.first.second, pos.second.first / dlen, pos.second.second / dlen);
        } else {
            teleport_to(id, pos.first.first, pos.first.second, 0, 0);
        }
    }
    if (finished) {
        auto obj = get_object(id);
        obj->set_state(SOS_LOADING);
        obj->set_velocity(0, 0);
        obj->set_remain_loading_time(5.0f);
        r->reverse();
    }
}
