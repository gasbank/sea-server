#pragma once
#include "sea-object.hpp"

namespace ss {
    namespace bg = boost::geometry;
    namespace bgi = boost::geometry::index;

    class sea {
        typedef bg::model::point<float, 2, bg::cs::cartesian> point;
        typedef bg::model::box<point> box;
        typedef std::pair<box, int> value;

    public:
        sea();
        void populate_test();
        int spawn(int type, float x, float y, float w, float h);
        int spawn(const char* guid, int type, float x, float y, float w, float h);
        void travel_to(const char* guid, float x, float y, float v = 1.0f);
        void teleport_to(int id, float x, float y, float vx = 0, float vy = 0);
        void teleport_to(const char* guid, float x, float y, float vx = 0, float vy = 0);
        void teleport_by(const char* guid, float dx, float dy);
        void query_near_lng_lat_to_packet(float lng, float lat, short halfex, std::vector<sea_object_public>& sop_list) const;
        void query_near_to_packet(float xc, float yc, float ex, std::vector<sea_object_public>& sop_list) const;
        void update(float delta_time);
        
    private:
        float lng_to_xc(float lng) const;
        float lat_to_yc(float lat) const;
        std::vector<int> query_tree(float xc, float yc, float ex) const;

        std::unordered_map<int, sea_object> sea_objects;
        std::unordered_map<std::string, int> sea_guid_to_id;
        bgi::rtree< value, bgi::quadratic<16> > rtree;
        const short res_width;
        const short res_height;
        const float km_per_cell;
    };
}
