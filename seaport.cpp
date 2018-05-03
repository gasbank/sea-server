#include "precompiled.hpp"
#include "seaport.hpp"
#include "xy.hpp"

#define SEAPORT_RTREE_FILENAME "rtree/seaport.dat"
#define SEAPORT_RTREE_MMAP_MAX_SIZE (4 * 1024 * 1024)

using namespace ss;

int seaport::lng_to_xc(float lng) const {
    //return static_cast<int>(roundf(res_width / 2 + lng / 180.0f * res_width / 2)) & (res_width - 1);
    return static_cast<int>(roundf(res_width / 2 + lng / 180.0f * res_width / 2));
}

int seaport::lat_to_yc(float lat) const {
    //return static_cast<int>(roundf(res_height / 2 - lat / 90.0f * res_height / 2)) & (res_height - 1);
    return static_cast<int>(roundf(res_height / 2 - lat / 90.0f * res_height / 2));
}

//std::vector<seaport_object_public> seaport::query_near_lng_lat_to_packet(float lng, float lat, int half_lng_ex, int half_lat_ex) const {
//    return query_near_to_packet(lng_to_xc(lng),
//                                lat_to_yc(lat),
//                                half_lng_ex,
//                                half_lat_ex);
//}

std::vector<seaport_object_public> seaport::query_near_to_packet(int xc, int yc, float ex_lng, float ex_lat) const {
    const auto half_lng_ex = boost::math::iround(ex_lng / 2);
    const auto half_lat_ex = boost::math::iround(ex_lat / 2);
    auto values = query_tree_ex(xc, yc, half_lng_ex, half_lat_ex);
    std::vector<seaport_object_public> sop_list;
    for (std::size_t i = 0; i < values.size(); i++) {
        sop_list.emplace_back(seaport_object_public(values[i]));
    }
    return sop_list;
}

std::vector<seaport_object_public::value_t> seaport::query_tree_ex(int xc, int yc, int half_lng_ex, int half_lat_ex) const {
    seaport_object_public::box_t query_box(seaport_object_public::point_t(xc - half_lng_ex, yc - half_lat_ex), seaport_object_public::point_t(xc + half_lng_ex, yc + half_lat_ex));
    std::vector<seaport_object_public::value_t> result_s;
    rtree_ptr->query(bgi::intersects(query_box), std::back_inserter(result_s));
    return result_s;
}

typedef struct _LWTTLDATA_SEAPORT {
    char name[80]; // maximum length from crawling data: 65
    char locode[8]; // fixed length of 5
    float lat;
    float lng;
} LWTTLDATA_SEAPORT;

constexpr long long get_monotonic_uptime() {
    return std::chrono::steady_clock::now().time_since_epoch().count();
}

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
        //id_point[i] = seaport_object_public::point_t(lng_to_xc(sp[i].lng), lat_to_yc(sp[i].lat));
    }

    const auto monotonic_uptime = get_monotonic_uptime();

    std::set<std::pair<int,int> > point_set;
    std::vector<seaport_object_public::value_t> duplicates;
    const auto bounds = rtree_ptr->bounds();
    for (auto it = rtree_ptr->qbegin(bgi::intersects(bounds)); it != rtree_ptr->qend(); it++) {
        const auto xc0 = it->first.get<0>();
        const auto yc0 = it->first.get<1>();
        const auto point = std::make_pair(xc0, yc0);
        if (point_set.find(point) == point_set.end()) {
            id_point[it->second] = it->first;

            update_chunk_key_ts(xc0, yc0);
            
            point_set.insert(point);
        } else {
            duplicates.push_back(*it);
        }
    }
    for (const auto it : duplicates) {
        rtree_ptr->remove(it);
    }
    if (point_set.size() != rtree_ptr->size()) {
        LOGE("Seaport rtree integrity check failure. Point set size %1% != R tree size %2%",
             point_set.size(),
             rtree_ptr->size());
        abort();
    }

    // TESTING-----------------
    int i = count;
    seaport_object_public::point_t origin_port{ 0,0 };
    std::vector<seaport_object_public::value_t> to_be_removed;
    rtree_ptr->query(bgi::intersects(seaport_object_public::box_t{ {-32,-32},{32,32} }), std::back_inserter(to_be_removed));
    for (auto e : to_be_removed) {
        rtree_ptr->remove(e);
    }
    rtree_ptr->insert(std::make_pair(origin_port, i));
    id_name[i] = "Origin Port";
    name_id["Origin Port"] = i;
    id_point[i] = origin_port;
    count++;
    // TESTING-----------------
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

seaport_object_public::point_t seaport::get_seaport_point(int id) const {
    if (id >= 0) {
        auto cit = id_point.find(id);
        if (cit != id_point.end()) {
            return cit->second;
        }
    }
    return seaport_object_public::point_t(-1, -1);
}

seaport_object_public::point_t seaport::get_seaport_point(const char* name) const {
    auto id = get_seaport_id(name);
    return get_seaport_point(id);
}

int seaport::get_nearest_two(const xy32& pos, int& id1, std::string& name1, int& id2, std::string& name2) const {
    id1 = -1;
    id2 = -1;
    seaport_object_public::point_t p = { boost::numeric_cast<int>(pos.x), boost::numeric_cast<int>(pos.y) };
    int count = 0;
    for (auto it = rtree_ptr->qbegin(bgi::nearest(p, 2)); it != rtree_ptr->qend(); it++) {
        if (count == 0) {
            id1 = it->second;
            name1 = get_seaport_name(it->second);
            LOGI("Nearest 1: %1% (%2%,%3%)", name1, it->first.get<0>(), it->first.get<1>());
            count++;
        } else if (count == 1) {
            id2 = it->second;
            name2 = get_seaport_name(it->second);
            LOGI("Nearest 2: %1% (%2%,%3%)", name2, it->first.get<0>(), it->first.get<1>());
            count++;
            return count;
        }
    }
    return 0;
}

void seaport::update_chunk_key_ts(int xc0, int yc0) {
    int view_scale = LNGLAT_VIEW_SCALE_PING_MAX;
    const auto monotonic_uptime = get_monotonic_uptime();
    while (view_scale) {
        const auto xc0_aligned = aligned_chunk_index(xc0, view_scale, LNGLAT_SEA_PING_EXTENT_IN_CELL_PIXELS);
        const auto yc0_aligned = aligned_chunk_index(yc0, view_scale, LNGLAT_SEA_PING_EXTENT_IN_CELL_PIXELS);
        const auto chunk_key = make_chunk_key(xc0_aligned, yc0_aligned, view_scale);
        auto it = chunk_key_ts.find(chunk_key.v);
        if (it != chunk_key_ts.end()) {
            it->second++;
        } else {
            chunk_key_ts[chunk_key.v] = monotonic_uptime;
        }
        view_scale >>= 1;
    }
}

int seaport::spawn(const char* name, int xc0, int yc0) {
    seaport_object_public::point_t new_port_point{ xc0, yc0 };
    if (rtree_ptr->qbegin(bgi::intersects(new_port_point)) != rtree_ptr->qend()) {
        // already exists
        return -1;
    }

    const auto id = static_cast<int>(rtree_ptr->size());
    rtree_ptr->insert(std::make_pair(new_port_point, id));
    id_point[id] = new_port_point;
    if (name[0] != 0) {
        set_name(id, name);
    } else {
        LOGE("%1%: seaport spawned, but name empty. (seaport id = %2%)",
             __func__,
             id);
    }

    update_chunk_key_ts(xc0, yc0);
    return id;
}

void seaport::despawn(int id) {
    auto it = id_point.find(id);
    if (it == id_point.end()) {
        LOGE("%1%: id not found (%2%)",
             __func__,
             id);
        return;
    }
    
    const auto xc0 = it->second.get<0>();
    const auto yc0 = it->second.get<1>();
    update_chunk_key_ts(xc0, yc0);

    rtree_ptr->remove(std::make_pair(it->second, id));
    id_point.erase(it);
    id_name.erase(id);
    //name_id
}

void seaport::set_name(int id, const char* name) {
    if (id_point.find(id) != id_point.end()) {
        id_name[id] = name;
        name_id[name] = id;
    } else {
        LOGE("%1%: cannot find seaport id %2%. seaport set name to '%3%' failed.",
             __func__,
             id,
             name);
    }
}

long long seaport::query_ts(const int xc0, const int yc0, const int view_scale) const {
    return query_ts(make_chunk_key(xc0, yc0, view_scale));
}

long long seaport::query_ts(const LWTTLCHUNKKEY chunk_key) const {
    const auto cit = chunk_key_ts.find(chunk_key.v);
    if (cit != chunk_key_ts.cend()) {
        return cit->second;
    }
    return 0;
}
