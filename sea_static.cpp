#include "precompiled.hpp"
#include "sea_static.hpp"

#define WORLDMAP_RTREE_FILENAME "rtree/worldmap_static.dat"
#define WORLDMAP_RTREE_MMAP_MAX_SIZE (7 * 1024 * 1024)

using namespace ss;

short sea_static::lng_to_xc(float lng) const {
    return static_cast<short>(roundf(res_width / 2 + lng / 180.0f * res_width / 2)) & (res_width - 1);
}

short sea_static::lat_to_yc(float lat) const {
    return static_cast<short>(roundf(res_height / 2 - lat / 90.0f * res_height / 2)) & (res_height - 1);
}

std::vector<sea_static_object_public> sea_static::query_near_lng_lat_to_packet(float lng, float lat, short halfex) const {
    return query_near_to_packet(lng_to_xc(lng), lat_to_yc(lat), halfex);
}

std::vector<sea_static_object_public> sea_static::query_near_to_packet(short xc, short yc, short halfex) const {
    auto values = query_tree(xc, yc, halfex);
    std::vector<sea_static_object_public> sop_list;
    for (std::size_t i = 0; i < values.size(); i++) {
        sop_list.emplace_back(sea_static_object_public(values[i]));
    }
    return sop_list;
}

std::vector<sea_static_object_public::value_t> sea_static::query_tree(short xc, short yc, short halfex) const {
    sea_static_object_public::box_t query_box(sea_static_object_public::point_t(xc - halfex, yc - halfex), sea_static_object_public::point_t(xc + halfex, yc + halfex));
    std::vector<sea_static_object_public::value_t> result_s;
    rtree_ptr->query(bgi::intersects(query_box), std::back_inserter(result_s));
    return result_s;
}

sea_static::sea_static()
    : file(bi::open_or_create, WORLDMAP_RTREE_FILENAME, WORLDMAP_RTREE_MMAP_MAX_SIZE)
    , alloc(file.get_segment_manager())
    , rtree_ptr(file.find_or_construct<sea_static_object_public::rtree_t>("rtree")(sea_static_object_public::params_t(), sea_static_object_public::indexable_t(), sea_static_object_public::equal_to_t(), alloc))
    , res_width(1 << 14) // 16384
    , res_height(1 << 13) // 8192
    , km_per_cell(40075.0f / res_width) {}
