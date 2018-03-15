#include "precompiled.hpp"
#include "sea.hpp"

using namespace ss;

void sea::populate_test() {
    boost::random::mt19937 rng;
    boost::random::uniform_real_distribution<float> random_point(-2500, 2500);
    for (int i = 0; i < 25000; i++) {
        float x = random_point(rng);
        float y = random_point(rng);
        spawn(i + 1, x, y, 1, 1);
        if ((i + 1) % 5000 == 0) {
            std::cout << "Spawning " << (i + 1) << "..." << std::endl;
        }
    }
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
    int id = sea_objects.size() + 1;
    box b(point(x, y), point(x + w, y + h));
    auto rtree_value = std::make_pair(b, id);
    sea_objects.emplace(std::pair<int, sea_object>(id, sea_object(id, type, x, y, w, h, rtree_value)));
    rtree.insert(rtree_value);
    return id;
}

void sea::teleport_to(const char* guid, float x, float y) {
    auto it = sea_guid_to_id.find(guid);
    if (it != sea_guid_to_id.end()) {
        auto it2 = sea_objects.find(it->second);
        if (it2 != sea_objects.end()) {
            rtree.remove(it2->second.get_rtree_value());
            it2->second.set_xy(x, y);
            rtree.insert(it2->second.get_rtree_value());
        } else {
            std::cerr << boost::format("Sea object not found corresponding to guid %1%\n") % guid;
        }
    } else {
        std::cerr << boost::format("Sea object ID not found corresponding to guid %1%\n") % guid;
    }
}

void sea::query_near_to_packet(float xc, float yc, float ex, std::vector<sea_object_public>& sop_list) {
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
