#pragma once

namespace ss {
    namespace bi = boost::interprocess;
    namespace bg = boost::geometry;
    namespace bgm = boost::geometry::model;
    namespace bgi = boost::geometry::index;

    struct seaarea_rtree {
        typedef bgm::point<float, 2, bg::cs::cartesian> point_t;
        typedef bgm::box<point_t> box_t;
        typedef std::pair<box_t, int> value_t;
        typedef bgi::linear<32, 8> params_t;
        typedef bgi::indexable<value_t> indexable_t;
        typedef bgi::equal_to<value_t> equal_to_t;
        typedef bi::allocator<value_t, bi::managed_mapped_file::segment_manager> allocator_t;
        typedef bgi::rtree<value_t, params_t, indexable_t, equal_to_t, allocator_t> rtree_t;
    };

    class seaarea {
    public:
        seaarea(const std::string& rtree_filename, size_t mmap_max_size, const std::string& source_filename);
        bool seaarea::query_tree(float lng, float lat, std::string& name) const;
        struct seaarea_entry {
            std::string name;
            float xmin;
            float ymin;
            float xmax;
            float ymax;
            int parts;
            int points;
            const int* parts_array;
            const float* lng_array;
            const float* lat_array;
        };
    private:
        seaarea();
        seaarea(seaarea&);
        seaarea(seaarea&&);

        static bool query_rtree(seaarea_rtree::rtree_t* rtree, const std::vector<seaarea::seaarea_entry>& entries, float lng, float lat, std::string& name);
        static int contains_point_in_polygon(const seaarea_entry& ent, float testx, float testy);

        bi::managed_mapped_file seaarea_rtree_file;
        seaarea_rtree::allocator_t seaarea_alloc;
        seaarea_rtree::rtree_t* seaarea_rtree;
        boost::interprocess::file_mapping seaarea_file;
        boost::interprocess::mapped_region seaarea_region;
        std::vector<seaarea_entry> seaarea_entries;
    };
}
