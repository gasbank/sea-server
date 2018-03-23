#pragma once

namespace ss {
    namespace bi = boost::interprocess;
    namespace bg = boost::geometry;
    namespace bgm = boost::geometry::model;
    namespace bgi = boost::geometry::index;

    struct seaport_object_public {
        typedef bgm::point<short, 2, bg::cs::cartesian> point_t;
        typedef bgm::box<point_t> box_t; // only for query
        typedef std::pair<point_t, int> value_t;
        typedef bgi::linear<32, 8> params_t;
        typedef bgi::indexable<value_t> indexable_t;
        typedef bgi::equal_to<value_t> equal_to_t;
        typedef bi::allocator<value_t, bi::managed_mapped_file::segment_manager> allocator_t;
        typedef bgi::rtree<value_t, params_t, indexable_t, equal_to_t, allocator_t> rtree_t;

        short x0, y0;
        int id;

        seaport_object_public(const value_t& v)
            : x0(v.first.get<0>())
            , y0(v.first.get<1>())
            , id(v.second) {
        }
    };
}
