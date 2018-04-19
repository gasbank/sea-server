#pragma once

#include "seaport_object.hpp"

struct xy32;
namespace ss {
    class seaport {
    public:
        std::vector<seaport_object_public> query_near_lng_lat_to_packet(float lng, float lat, int halfex) const;
        const char* get_seaport_name(int id) const;
        int get_seaport_id(const char* name) const;
        seaport_object_public::point_t get_seaport_point(int id) const;
        seaport_object_public::point_t get_seaport_point(const char* name) const;
        seaport();
        int get_nearest_two(const xy32& pos, int& id1, std::string& name1, int& id2, std::string& name2) const;
    private:
        std::vector<seaport_object_public> query_near_to_packet(int xc, int yc, int halfex) const; 
        std::vector<seaport_object_public::value_t> query_tree(int xc, int yc, int halfex) const;
        int lng_to_xc(float lng) const;
        int lat_to_yc(float lat) const;
        bi::managed_mapped_file file;
        seaport_object_public::allocator_t alloc;
        seaport_object_public::rtree_t* rtree_ptr;
        const int res_width;
        const int res_height;
        const float km_per_cell;
        std::unordered_map<int, std::string> id_name;
        std::unordered_map<int, seaport_object_public::point_t> id_point;
        std::unordered_map<std::string, int> name_id;
    };
}
