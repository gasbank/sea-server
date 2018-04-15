#pragma once

#include "sea_static_object.hpp"

struct xy32;

namespace ss {
    class sea_static {
    public:
        std::vector<sea_static_object_public> query_near_lng_lat_to_packet(float lng, float lat, int halfex) const;
        
        sea_static();
        std::vector<xy32> calculate_waypoints(const xy32& from, const xy32& to) const;
        std::vector<xy32> calculate_waypoints(const sea_static_object_public::point_t& from, const sea_static_object_public::point_t& to) const;
        bool is_water(const xy32& cell) const;
    private:
        std::vector<sea_static_object_public> query_near_to_packet(int xc, int yc, int halfex) const; 
        std::vector<sea_static_object_public::value_t> query_tree(int xc, int yc, int halfex) const;
        int lng_to_xc(float lng) const;
        int lat_to_yc(float lat) const;
        bi::managed_mapped_file land_file;
        sea_static_object_public::allocator_t land_alloc;
        sea_static_object_public::rtree_t* land_rtree_ptr;
        bi::managed_mapped_file water_file;
        sea_static_object_public::allocator_t water_alloc;
        sea_static_object_public::rtree_t* water_rtree_ptr;
        const int res_width;
        const int res_height;
        const float km_per_cell;
    };
}
