#pragma once

#include "seaport_object.hpp"

struct xy32;
namespace ss {
    class seaport {
    public:
        //std::vector<seaport_object_public> query_near_lng_lat_to_packet(float lng, float lat, int half_lng_ex, int half_lat_ex) const;
        std::vector<seaport_object_public> query_near_to_packet(int xc, int yc, float ex_lng, float ex_lat) const;
        const char* get_seaport_name(int id) const;
        int get_seaport_id(const char* name) const;
        seaport_object_public::point_t get_seaport_point(int id) const;
        seaport_object_public::point_t get_seaport_point(const char* name) const;
        seaport();
        int get_nearest_two(const xy32& pos, int& id1, std::string& name1, int& id2, std::string& name2) const;
        int lng_to_xc(float lng) const;
        int lat_to_yc(float lat) const;
        int spawn(const char* name, int xc0, int yc0);
        void despawn(int id);
        void set_name(int id, const char* name);
        long long query_ts(const int xc0, const int yc0, const int view_scale) const;
        long long query_ts(const LWTTLCHUNKKEY chunk_key) const;
    private:
        std::vector<seaport_object_public::value_t> query_tree_ex(int xc, int yc, int half_lng_ex, int half_lat_ex) const;
        void update_chunk_key_ts(int xc0, int yc0);
        bi::managed_mapped_file file;
        seaport_object_public::allocator_t alloc;
        seaport_object_public::rtree_t* rtree_ptr;
        const int res_width;
        const int res_height;
        const float km_per_cell;
        std::unordered_map<int, std::string> id_name; // seaport ID -> seaport name
        std::unordered_map<int, seaport_object_public::point_t> id_point; // seaport ID -> seaport position
        std::unordered_map<std::string, int> name_id; // seaport name -> seaport ID (XXX NOT UNIQUE XXX)
        std::unordered_map<int, long long> chunk_key_ts; // chunk key -> timestamp
    };
}
