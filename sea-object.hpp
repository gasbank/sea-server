#pragma once

namespace ss {
    namespace bg = boost::geometry;
    namespace bgi = boost::geometry::index;

    struct sea_object_public {
        int id;
        int type;
        float x, y;
        float w, h;
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
            rtree_value(rtree_value)
        {
        }
        void fill_sop(sea_object_public& sop) {
            sop.x = x;
            sop.y = y;
            sop.w = w;
            sop.h = h;
            sop.id = id;
            sop.type = type;
        }
        void set_guid(const std::string& v) {
            guid = v;
        }
        void set_xy(float xv, float yv) {
            x = xv;
            y = yv;
            rtree_value.first.min_corner().set<0>(x);
            rtree_value.first.min_corner().set<1>(y);
            rtree_value.first.max_corner().set<0>(x + w);
            rtree_value.first.max_corner().set<1>(y + h);
        }
        const value& get_rtree_value() const { return rtree_value; }
    private:
        explicit sea_object() {}
        int id;
        int type;
        float x, y;
        float w, h;
        std::string guid;
        value rtree_value;
    };
}
