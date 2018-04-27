#include "precompiled.hpp"
#include "sea_static.hpp"
#include "astarrtree.hpp"

#define WORLDMAP_RTREE_MMAP_MAX_SIZE_RATIO (sizeof(size_t) / 4)
#define WORLDMAP_RTREE_MMAP_MAX_SIZE(mb) ((mb) * 1024 * 1024 * WORLDMAP_RTREE_MMAP_MAX_SIZE_RATIO)
#define DATA_ROOT "rtree/"
#define WORLDMAP_LAND_MAX_RECT_RTREE_RTREE_FILENAME "worldmap_land_max_rect.dat"
#define WORLDMAP_LAND_MAX_RECT_RTREE_MMAP_MAX_SIZE WORLDMAP_RTREE_MMAP_MAX_SIZE(300)
#define WORLDMAP_WATER_MAX_RECT_RTREE_RTREE_FILENAME "worldmap_water_max_rect.dat"
#define WORLDMAP_WATER_MAX_RECT_RTREE_MMAP_MAX_SIZE WORLDMAP_RTREE_MMAP_MAX_SIZE(300)
#define SEA_WATER_SET_FILENAME "sea_water_set.dat"
#define SEA_WATER_SET_MMAP_MAX_SIZE WORLDMAP_RTREE_MMAP_MAX_SIZE(90)

using namespace ss;

int sea_static::lng_to_xc(float lng) const {
    //return static_cast<int>(roundf(res_width / 2 + lng / 180.0f * res_width / 2)) & (res_width - 1);
    return static_cast<int>(roundf(res_width / 2 + lng / 180.0f * res_width / 2));
}

int sea_static::lat_to_yc(float lat) const {
    //return static_cast<int >(roundf(res_height / 2 - lat / 90.0f * res_height / 2)) & (res_height - 1);
    return static_cast<int>(roundf(res_height / 2 - lat / 90.0f * res_height / 2));
}

std::vector<sea_static_object_public> sea_static::query_near_lng_lat_to_packet(float lng, float lat, float ex) const {
    return query_near_to_packet(lng_to_xc(lng), lat_to_yc(lat), ex);
}

std::vector<sea_static_object_public> sea_static::query_near_to_packet(int xc, int yc, float ex) const {
    const auto halfex = boost::math::iround(ex / 2);
    auto values = query_tree(xc, yc, halfex);
    std::vector<sea_static_object_public> sop_list;
    for (std::size_t i = 0; i < values.size(); i++) {
        sop_list.emplace_back(sea_static_object_public(values[i]));
    }
    return sop_list;
}

std::vector<sea_static_object_public> sea_static::query_near_to_packet(int xc0, int yc0, int xc1, int yc1) const {
    auto values = query_tree(xc0, yc0, xc1, yc1);
    std::vector<sea_static_object_public> sop_list;
    for (std::size_t i = 0; i < values.size(); i++) {
        sop_list.emplace_back(sea_static_object_public(values[i]));
    }
    return sop_list;
}

std::vector<sea_static_object_public::value_t> sea_static::query_tree(int xc, int yc, int halfex) const {
    return query_tree(xc - halfex, yc - halfex, xc + halfex, yc + halfex);
}

std::vector<sea_static_object_public::value_t> sea_static::query_tree(int xc0, int yc0, int xc1, int yc1) const {
    sea_static_object_public::box_t query_box(sea_static_object_public::point_t(xc0, yc0), sea_static_object_public::point_t(xc1, yc1));
    std::vector<sea_static_object_public::value_t> result_s;
    land_rtree_ptr->query(bgi::intersects(query_box), std::back_inserter(result_s));
    return result_s;
}

void load_from_dump_if_empty(sea_static_object_public::rtree_t* rtree_ptr, const char* dump_filename) {
    if (rtree_ptr->size() == 0) {
        LOGI("R-tree empty. Trying to create R-tree from dump file %s...",
             dump_filename);
        int rect_count = 0;
        FILE* fin = fopen(dump_filename, "rb");
        if (fin) {
            size_t read_max_count = 100000; // elements
            void* read_buf = malloc(sizeof(xy32xy32) * read_max_count);
            fseek(fin, 0, SEEK_SET);
            while (size_t read_count = fread(read_buf, sizeof(xy32xy32), read_max_count, fin)) {
                for (size_t i = 0; i < read_count; i++) {
                    rect_count++;
                    xy32xy32* r = reinterpret_cast<xy32xy32*>(read_buf) + i;
                    sea_static_object_public::box_t box(sea_static_object_public::point_t(r->xy0.x, r->xy0.y), sea_static_object_public::point_t(r->xy1.x, r->xy1.y));
                    rtree_ptr->insert(std::make_pair(box, rect_count));
                }
            }
            fclose(fin);
            LOGI("Max rect R Tree size (after loaded from %1%): %2%", dump_filename, rtree_ptr->size());
        } else {
            LOGE("[Error] Dump file %s not exist!", dump_filename);
        }
    }
}

void sea_static::mark_sea_water(sea_static_object_public::rtree_t* rtree) {
    const char* sea_water_dump_filename = "rtree/sea_water_dump.dat";
    LOGI("Checking %1%...", sea_water_dump_filename);
    struct stat stat_buffer;
    if (stat(sea_water_dump_filename, &stat_buffer) != 0) {
        std::unordered_set<int> sea_water_set;
        LOGI("%1% not exists. Creating...", sea_water_dump_filename);
        LOGI("R-tree total node: %1%", rtree->size());
        std::vector<sea_static_object_public::rtree_t::const_query_iterator> open_set;
        for (auto it = rtree->qbegin(bgi::intersects(astarrtree::box_t_from_xy(xy32{ 0,0 }))); it != rtree->qend(); it++) {
            sea_water_set.insert(it->second);
            open_set.push_back(it);
        }
        std::unordered_set<int> visited_set;
        while (open_set.size() > 0) {
            std::vector<sea_static_object_public::rtree_t::const_query_iterator> next_open_set;
            for (auto open_it : open_set) {
                for (auto it = rtree->qbegin(bgi::intersects(open_it->first)); it != rtree->qend(); it++) {
                    if (visited_set.find(it->second) == visited_set.end()) {
                        visited_set.insert(it->second);
                        sea_water_set.insert(it->second);
                        next_open_set.push_back(it);
                    }
                }
            }
            open_set = next_open_set;
            LOGI("Sea water set size: %1% (%2% %%)",
                 sea_water_set.size(),
                 ((float)sea_water_set.size() / rtree->size() * 100.0f));
        }
        sea_water_vector.assign(sea_water_set.begin(), sea_water_set.end());
        std::sort(sea_water_vector.begin(), sea_water_vector.end());
        FILE* sea_water_dump_file = fopen(sea_water_dump_filename, "wb");
        fwrite(&sea_water_vector[0], sizeof(int), sea_water_vector.size(), sea_water_dump_file);
        fclose(sea_water_dump_file);
        sea_water_dump_file = 0;
    } else {
        size_t count = stat_buffer.st_size / sizeof(int);
        LOGI("Sea water dump count: %1%", count);
        FILE* sea_water_dump_file = fopen(sea_water_dump_filename, "rb");
        sea_water_vector.resize(count);
        auto read_sea_water_vector_count = fread(&sea_water_vector[0], sizeof(int), sea_water_vector.size(), sea_water_dump_file);
        if (read_sea_water_vector_count != sea_water_vector.size()) {
            LOGE("Sea water dump file read count error! (Expected: %1%, Actual: %2%) Aborting...",
                 sea_water_vector.size(),
                 read_sea_water_vector_count);
            abort();
        }
        LOGI("Sea water vector count: %1%", sea_water_vector.size());
    }
}

sea_static::sea_static()
    : land_file(bi::open_or_create, DATA_ROOT WORLDMAP_LAND_MAX_RECT_RTREE_RTREE_FILENAME, WORLDMAP_LAND_MAX_RECT_RTREE_MMAP_MAX_SIZE)
    , land_alloc(land_file.get_segment_manager())
    , land_rtree_ptr(land_file.find_or_construct<sea_static_object_public::rtree_t>("rtree")(sea_static_object_public::params_t(), sea_static_object_public::indexable_t(), sea_static_object_public::equal_to_t(), land_alloc))
    , water_file(bi::open_or_create, DATA_ROOT WORLDMAP_WATER_MAX_RECT_RTREE_RTREE_FILENAME, WORLDMAP_WATER_MAX_RECT_RTREE_MMAP_MAX_SIZE)
    , water_alloc(water_file.get_segment_manager())
    , water_rtree_ptr(water_file.find_or_construct<sea_static_object_public::rtree_t>("rtree")(sea_static_object_public::params_t(), sea_static_object_public::indexable_t(), sea_static_object_public::equal_to_t(), water_alloc))
    , res_width(WORLD_MAP_PIXEL_RESOLUTION_WIDTH)
    , res_height(WORLD_MAP_PIXEL_RESOLUTION_HEIGHT)
    , km_per_cell(WORLD_CIRCUMFERENCE_IN_KM / res_width)
    //, sea_water_set_file(bi::open_or_create, DATA_ROOT SEA_WATER_SET_FILENAME, SEA_WATER_SET_MMAP_MAX_SIZE)
    //, sea_water_set_alloc(sea_water_set_file.get_segment_manager())
    //, sea_water_set(sea_water_set_file.find_or_construct<sea_water_set_t>("set")(int(), std::hash<int>(), std::equal_to<int>(), sea_water_set_alloc))
{

    load_from_dump_if_empty(land_rtree_ptr, "rtree/land_raw_xy32xy32.bin");
    load_from_dump_if_empty(water_rtree_ptr, "rtree/water_raw_xy32xy32.bin");
    mark_sea_water(water_rtree_ptr);

    // TESTING-----------------
    //sea_static_object_public::box_t origin_land_cell{ {-32,-32},{32,32} };
    sea_static_object_public::box_t origin_land_cell{ { 0,0 },{ 1,1 } };
    sea_static_object_public::box_t test_land_cell{ { 0,0 },{ 15,15 } };
    std::vector<sea_static_object_public::value_t> to_be_removed;
    land_rtree_ptr->query(bgi::contains(origin_land_cell), std::back_inserter(to_be_removed));
    land_rtree_ptr->query(bgi::contains(test_land_cell), std::back_inserter(to_be_removed));
    for (auto it : to_be_removed) {
        land_rtree_ptr->remove(it);
    }
    land_rtree_ptr->insert(std::make_pair(test_land_cell, -1));
    // TESTING-----------------
}

std::vector<xy32> sea_static::calculate_waypoints(const xy32 & from, const xy32 & to) const {
    auto from_box = astarrtree::box_t_from_xy(from);
    std::vector<astarrtree::value_t> from_result_s;
    water_rtree_ptr->query(bgi::contains(from_box), std::back_inserter(from_result_s));
    xy32 new_from = from;
    bool new_from_sea_water = false;
    if (astarrtree::find_nearest_point_if_empty(water_rtree_ptr, new_from, from_box, from_result_s)) {
        new_from_sea_water = is_sea_water(xy32{ new_from.x, new_from.y });
        LOGI("  'From' point changed to (%1%,%2%) [sea water=%3%]",
             new_from.x,
             new_from.y,
             new_from_sea_water);
    } else {
        new_from_sea_water = is_sea_water(xy32{ new_from.x, new_from.y });
        LOGI("  'From' point (%1%,%2%) [sea water=%3%]",
             new_from.x,
             new_from.y,
             new_from_sea_water);
    }

    auto to_box = astarrtree::box_t_from_xy(to);
    std::vector<astarrtree::value_t> to_result_s;
    water_rtree_ptr->query(bgi::contains(to_box), std::back_inserter(to_result_s));
    xy32 new_to = to;
    bool new_to_sea_water = false;
    if (astarrtree::find_nearest_point_if_empty(water_rtree_ptr, new_to, to_box, to_result_s)) {
        new_to_sea_water = is_sea_water(xy32{ new_to.x, new_to.y });
        LOGI("  'To' point changed to (%1%,%2%) [sea water=%3%]",
             new_to.x,
             new_to.y,
             new_to_sea_water);
    } else {
        new_to_sea_water = is_sea_water(xy32{ new_to.x, new_to.y });
        LOGI("  'To' point (%1%,%2%) [sea water=%3%]",
             new_to.x,
             new_to.y,
             new_to_sea_water);
    }

    if (new_from_sea_water && new_to_sea_water) {
        return astarrtree::astar_rtree_memory(water_rtree_ptr, new_from, new_to);
    } else {
        LOGE("ERROR: Both 'From' and 'To' should be in sea water to generate waypoints!");
        return std::vector<xy32>();
    }
}

std::vector<xy32> sea_static::calculate_waypoints(const sea_static_object_public::point_t & from, const sea_static_object_public::point_t & to) const {
    xy32 fromxy;
    xy32 toxy;
    fromxy.x = from.get<0>();
    fromxy.y = from.get<1>();
    toxy.x = to.get<0>();
    toxy.y = to.get<1>();
    return calculate_waypoints(fromxy, toxy);
}

bool sea_static::is_water(const xy32& cell) const {
    auto cell_box = astarrtree::box_t_from_xy(cell);
    return water_rtree_ptr->qbegin(bgi::contains(cell_box)) != water_rtree_ptr->qend();
}

bool sea_static::is_sea_water(const xy32& cell) const {
    auto cell_box = astarrtree::box_t_from_xy(cell);
    auto it = water_rtree_ptr->qbegin(bgi::contains(cell_box));
    if (it != water_rtree_ptr->qend()) {
        return std::binary_search(sea_water_vector.begin(), sea_water_vector.end(), it->second);
    }
    return false;
}
