#pragma once

#include "sea_static_object.hpp"

struct xy32;
namespace ss {
    class sea_static {
    public:
        std::vector<sea_static_object_public> query_near_lng_lat_to_packet(float lng, float lat, float ex) const;
        std::vector<sea_static_object_public> query_near_to_packet(int xc, int yc, float ex_lng, float ex_lat) const;
        std::vector<sea_static_object_public> query_near_to_packet(int xc0, int yc0, int xc1, int yc1) const;
        sea_static();
        std::vector<xy32> calculate_waypoints(const xy32& from, const xy32& to) const;
        std::vector<xy32> calculate_waypoints(const sea_static_object_public::point_t& from, const sea_static_object_public::point_t& to) const;
        bool is_water(const xy32& cell) const;
        bool is_sea_water(const xy32& cell) const;
        int lng_to_xc(float lng) const;
        int lat_to_yc(float lat) const;
        long long query_ts(const int xc0, const int yc0, const int view_scale) const;
        long long query_ts(const LWTTLCHUNKKEY& chunk_key) const;
        unsigned int query_single_cell(int xc0, int yc0) const;
    private:
        std::vector<sea_static_object_public::value_t> query_tree_ex(int xc, int yc, int half_lng_ex, int half_lat_ex) const;
        std::vector<sea_static_object_public::value_t> query_tree(int xc0, int yc0, int xc1, int yc1) const;
        void mark_sea_water(sea_static_object_public::rtree_t* rtree);
        void update_chunk_key_ts(int xc0, int yc0);
        bi::managed_mapped_file land_file;
        sea_static_object_public::allocator_t land_alloc;
        sea_static_object_public::rtree_t* land_rtree_ptr;
        bi::managed_mapped_file water_file;
        sea_static_object_public::allocator_t water_alloc;
        sea_static_object_public::rtree_t* water_rtree_ptr;

        //bi::managed_mapped_file sea_water_set_file;
        //sea_static_object_public::allocator_t sea_water_set_alloc;

        const int res_width;
        const int res_height;
        const float km_per_cell;
        //typedef std::unordered_set<int, std::hash<int>, std::equal_to<int>, sea_static_object_public::allocator_t> sea_water_set_t;
        //sea_water_set_t* sea_water_set;
        std::vector<int> sea_water_vector;
        std::unordered_map<int, long long> chunk_key_ts; // chunk key -> timestamp
    };
}
