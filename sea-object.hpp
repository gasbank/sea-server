#pragma once

#include "container.hpp"

namespace ss {
    namespace bg = boost::geometry;
    namespace bgi = boost::geometry::index;

    struct sea_object_public {
        int id;
        int type;
        float x, y;
        float w, h;
        float vx, vy;
        char guid[64];
    };

    enum SEA_OBJECT_STATE {
        SOS_SAILING,
        SOS_LOADING,
        SOS_UNLOADING,
        SOS_ERROR,
    };

    class sea_object {
        typedef bg::model::point<float, 2, bg::cs::cartesian> point;
        typedef bg::model::box<point> box;
        typedef std::pair<box, int> value;
    public:
        sea_object(int id, int type, float x, float y, float w, float h, const value& rtree_value)
            : id(id),
            type(type),
            x(x),
            y(y),
            w(w),
            h(h),
            vx(0),
            vy(0),
            rtree_value(rtree_value),
            state(SOS_SAILING),
            remain_loading_time(0) {
        }
        void fill_sop(sea_object_public& sop) const {
            sop.x = x + 0.5f;
            sop.y = y + 0.5f;
            sop.w = w;
            sop.h = h;
            sop.vx = vx;
            sop.vy = vy;
            sop.id = id;
            sop.type = type;
            strcpy(sop.guid, guid.c_str());
            if (state == SOS_LOADING) {
                strcat(sop.guid, "[LOADING]");
            } else if (state == SOS_UNLOADING) {
                strcat(sop.guid, "[UNLOADING]");
            }
        }
        void set_guid(const std::string& v) {
            guid = v;
        }
        void translate_xy(float dxv, float dyv) {
            set_xy(x + dxv, y + dyv);
        }
        void get_xy(float& xv, float& yv) const {
            xv = this->x;
            yv = this->y;
        }
        void set_xy(float xv, float yv) {
            x = xv;
            y = yv;
            rtree_value.first.min_corner().set<0>(x);
            rtree_value.first.min_corner().set<1>(y);
            rtree_value.first.max_corner().set<0>(x + w);
            rtree_value.first.max_corner().set<1>(y + h);
        }
        void set_velocity(float vx, float vy) {
            this->vx = vx;
            this->vy = vy;
        }
        void set_destination(float vx, float vy) {
            this->dest_x = vx;
            this->dest_y = vy;
        }
        void get_velocity(float& vx, float& vy) const {
            vx = this->vx;
            vy = this->vy;
        }
        float get_distance_to_destination() const {
            return sqrtf((dest_x - x) * (dest_x - x) + (dest_y - y) * (dest_y - y));
        }
        const value& get_rtree_value() const { return rtree_value; }
        void set_state(SEA_OBJECT_STATE state) { this->state = state; }
        SEA_OBJECT_STATE get_state() const { return state; }
        void set_remain_loading_time(float remain_loading_time) {
            this->remain_loading_time = remain_loading_time;
        }
        void update(float delta_time) {
            if (remain_loading_time > 0) {
                remain_loading_time -= delta_time;
                if (remain_loading_time <= 0) {
                    remain_loading_time = 0;
                    state = SOS_SAILING;
                }
            }
        }
        int get_type() const { return type; }
        int get_id() const { return id; }
    private:
        explicit sea_object() {}
        int id;
        int type;
        float x, y;
        float w, h;
        float vx, vy;
        float dest_x, dest_y;
        std::string guid;
        value rtree_value;
        SEA_OBJECT_STATE state;
        float remain_loading_time;
        std::vector<container> containers;
    };
}
