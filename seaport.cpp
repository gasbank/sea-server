#include "precompiled.hpp"
#include "seaport.hpp"
#include "xy.hpp"

#define SEAPORT_RTREE_FILENAME "rtree/seaport.dat"
#define SEAPORT_RTREE_MMAP_MAX_SIZE (7 * 1024 * 1024)

using namespace ss;

int seaport::lng_to_xc(float lng) const {
    return static_cast<int>(roundf(res_width / 2 + lng / 180.0f * res_width / 2)) & (res_width - 1);
}

int seaport::lat_to_yc(float lat) const {
    return static_cast<int>(roundf(res_height / 2 - lat / 90.0f * res_height / 2)) & (res_height - 1);
}

std::vector<seaport_object_public> seaport::query_near_lng_lat_to_packet(float lng, float lat, int halfex) const {
    return query_near_to_packet(lng_to_xc(lng), lat_to_yc(lat), halfex);
}

std::vector<seaport_object_public> seaport::query_near_to_packet(int xc, int yc, int halfex) const {
    auto values = query_tree(xc, yc, halfex);
    std::vector<seaport_object_public> sop_list;
    for (std::size_t i = 0; i < values.size(); i++) {
        sop_list.emplace_back(seaport_object_public(values[i]));
    }
    return sop_list;
}

std::vector<seaport_object_public::value_t> seaport::query_tree(int xc, int yc, int halfex) const {
    seaport_object_public::box_t query_box(seaport_object_public::point_t(xc - halfex, yc - halfex), seaport_object_public::point_t(xc + halfex, yc + halfex));
    std::vector<seaport_object_public::value_t> result_s;
    rtree_ptr->query(bgi::intersects(query_box), std::back_inserter(result_s));
    return result_s;
}

typedef struct _LWTTLDATA_SEAPORT {
    char locode[8];
    char name[64];
    float lat;
    float lng;
} LWTTLDATA_SEAPORT;

seaport::seaport()
    : file(bi::open_or_create, SEAPORT_RTREE_FILENAME, SEAPORT_RTREE_MMAP_MAX_SIZE)
    , alloc(file.get_segment_manager())
    , rtree_ptr(file.find_or_construct<seaport_object_public::rtree_t>("rtree")(seaport_object_public::params_t(), seaport_object_public::indexable_t(), seaport_object_public::equal_to_t(), alloc))
    , res_width(WORLD_MAP_PIXEL_RESOLUTION_WIDTH)
    , res_height(WORLD_MAP_PIXEL_RESOLUTION_HEIGHT)
    , km_per_cell(WORLD_CIRCUMFERENCE_IN_KM / res_width)
{
    boost::interprocess::file_mapping seaport_file("assets/ttldata/seaports.dat", boost::interprocess::read_only);
    boost::interprocess::mapped_region region(seaport_file, boost::interprocess::read_only);
    LWTTLDATA_SEAPORT* sp = reinterpret_cast<LWTTLDATA_SEAPORT*>(region.get_address());
    int count = static_cast<int>(region.get_size() / sizeof(LWTTLDATA_SEAPORT));
    // dump seaports.dat into r-tree data if r-tree is empty.
    if (rtree_ptr->size() == 0) {
        for (int i = 0; i < count; i++) {
            seaport_object_public::point_t point(lng_to_xc(sp[i].lng), lat_to_yc(sp[i].lat));
            rtree_ptr->insert(std::make_pair(point, i));
        }
    }
    for (int i = 0; i < count; i++) {
        id_name[i] = sp[i].name;
        name_id[sp[i].name] = i;
        id_point[i] = seaport_object_public::point_t(lng_to_xc(sp[i].lng), lat_to_yc(sp[i].lat));
    }
}

const char* seaport::get_seaport_name(int id) const {
    auto it = id_name.find(id);
    if (it != id_name.cend()) {
        return it->second.c_str();
    }
    return "";
}

int seaport::get_seaport_id(const char* name) const {
    auto it = name_id.find(name);
    if (it != name_id.cend()) {
        return it->second;
    }
    return -1;
}

seaport_object_public::point_t seaport::get_seaport_point(const char* name) const {
    auto id = get_seaport_id(name);
    if (id >= 0) {
        auto cit = id_point.find(id);
        if (cit != id_point.end()) {
            return cit->second;
        }
    }
    return seaport_object_public::point_t(-1, -1);
}

int seaport::get_nearest_two(const xy32& pos, std::string& name1, std::string& name2) const {
    seaport_object_public::point_t p = { boost::numeric_cast<int>(pos.x), boost::numeric_cast<int>(pos.y) };
    int count = 0;
    for (auto it = rtree_ptr->qbegin(bgi::nearest(p, 2)); it != rtree_ptr->qend(); it++) {
        if (count == 0) {
            name1 = get_seaport_name(it->second);
            count++;
        } else if (count == 1) {
            name2 = get_seaport_name(it->second);
            count++;
            return count;
        }
    }
    return 0;
}
