#pragma once

#include "sea_static_object.hpp"

namespace ss {
    class sea_static {
    public:
        std::vector<sea_static_object_public> query_near_lng_lat_to_packet(float lng, float lat, short halfex) const;
        
        sea_static();
    private:
        std::vector<sea_static_object_public> query_near_to_packet(short xc, short yc, short halfex) const; 
        std::vector<sea_static_object_public::value_t> query_tree(short xc, short yc, short halfex) const;
        short lng_to_xc(float lng) const;
        short lat_to_yc(float lat) const;
        bi::managed_mapped_file land_file;
        sea_static_object_public::allocator_t land_alloc;
        sea_static_object_public::rtree_t* land_rtree_ptr;
        bi::managed_mapped_file water_file;
        sea_static_object_public::allocator_t water_alloc;
        sea_static_object_public::rtree_t* water_rtree_ptr;
        const short res_width;
        const short res_height;
        const float km_per_cell;
    };
}
