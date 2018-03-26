#pragma once

namespace ss {
    namespace bg = boost::geometry;
    namespace bgi = boost::geometry::index;

    struct sea_object_public {
        int id;
        int type;
        float x, y;
        float w, h;
        float vx, vy;
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
            rtree_value(rtree_value)
        {
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
    };
}
