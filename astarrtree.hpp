#pragma once

#include "xy.hpp"

namespace astarrtree {
    namespace bi = boost::interprocess;
    namespace bg = boost::geometry;
    namespace bgm = bg::model;
    namespace bgi = bg::index;

    typedef bgm::point<int, 2, bg::cs::cartesian> point_t;
    typedef bgm::box<point_t> box_t;
    typedef std::pair<box_t, int> value_t;
    typedef bgi::linear<32, 8> params_t;
    typedef bgi::indexable<value_t> indexable_t;
    typedef bgi::equal_to<value_t> equal_to_t;
    typedef bi::allocator<value_t, bi::managed_mapped_file::segment_manager> allocator_t;
    typedef bgi::rtree<value_t, params_t, indexable_t, equal_to_t, allocator_t> rtree_t;

    void astar_rtree(const char* water_rtree_filename,
                     size_t water_output_max_size,
                     const char* land_rtree_filename,
                     size_t land_output_max_size,
                     xy32 from,
                     xy32 to);
    std::vector<xy32> astar_rtree_memory(rtree_t* rtree_water_ptr, rtree_t* rtree_land_ptr, xy32 from, xy32 to);
    box_t box_t_from_xy(xy32 v);
    bool find_nearest_point_if_empty(rtree_t* rtree_ptr, xy32& from, box_t& from_box, std::vector<value_t>& from_result_s);
}
