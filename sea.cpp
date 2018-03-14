#include "precompiled.hpp"
#include "sea.hpp"

using namespace ss;

void sea::populate_test() {
    boost::random::mt19937 rng;
    boost::random::uniform_real_distribution<float> random_point(-10000, 10000);
    for (int i = 0; i < 400000; i++) {
        float x = random_point(rng);
        float y = random_point(rng);
        spawn(i + 1, x, y, 1, 1);
        if ((i + 1) % 40000 == 0) {
            std::cout << "Spawning " << (i + 1) << "..." << std::endl;
        }
    }
    std::cout << "Spawning finished." << std::endl;
    std::vector<sea_object_public> sop_list;
    query_near_to_packet(0, 0, 10, sop_list);
}

int sea::spawn(int type, float x, float y, float w, float h) {
    int id = sea_objects.size() + 1;
    sea_objects.emplace(std::pair<int, sea_object>(id, sea_object(id, type, x, y, w, h)));
    box b(point(x, y), point(x + w, y + h));
    rtree.insert(std::make_pair(b, id));
    return id;
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
