#include "precompiled.hpp"
#include "sea_static.hpp"
#include "astarrtree.hpp"

#define WORLDMAP_RTREE_MMAP_MAX_SIZE_RATIO (sizeof(size_t) / 4)
#define WORLDMAP_RTREE_MMAP_MAX_SIZE(mb) ((mb) * 1024 * 1024 * WORLDMAP_RTREE_MMAP_MAX_SIZE_RATIO)
#define DATA_ROOT "rtree/"
#define WORLDMAP_LAND_MAX_RECT_RTREE_RTREE_FILENAME "worldmap_land_max_rect.dat"
#define WORLDMAP_LAND_MAX_RECT_RTREE_MMAP_MAX_SIZE WORLDMAP_RTREE_MMAP_MAX_SIZE(300)
#define WORLDMAP_WATER_MAX_RECT_RTREE_RTREE_FILENAME "worldmap_water_max_rect.dat"
#define WORLDMAP_WATER_MAX_RECT_RTREE_MMAP_MAX_SIZE WORLDMAP_RTREE_MMAP_MAX_SIZE(10)

using namespace ss;

int sea_static::lng_to_xc(float lng) const {
    //return static_cast<int>(roundf(res_width / 2 + lng / 180.0f * res_width / 2)) & (res_width - 1);
    return static_cast<int>(roundf(res_width / 2 + lng / 180.0f * res_width / 2));
}

int sea_static::lat_to_yc(float lat) const {
    //return static_cast<int >(roundf(res_height / 2 - lat / 90.0f * res_height / 2)) & (res_height - 1);
    return static_cast<int >(roundf(res_height / 2 - lat / 90.0f * res_height / 2));
}

std::vector<sea_static_object_public> sea_static::query_near_lng_lat_to_packet(float lng, float lat, float ex) const {
    return query_near_to_packet(lng_to_xc(lng), lat_to_yc(lat), static_cast<int>(roundf(ex / 2.0f)));
}

std::vector<sea_static_object_public> sea_static::query_near_to_packet(int xc, int yc, int halfex) const {
    auto values = query_tree(xc, yc, halfex);
    std::vector<sea_static_object_public> sop_list;
    for (std::size_t i = 0; i < values.size(); i++) {
        sop_list.emplace_back(sea_static_object_public(values[i]));
    }
    return sop_list;
}

std::vector<sea_static_object_public::value_t> sea_static::query_tree(int xc, int yc, int halfex) const {
    sea_static_object_public::box_t query_box(sea_static_object_public::point_t(xc - halfex, yc - halfex), sea_static_object_public::point_t(xc + halfex, yc + halfex));
    std::vector<sea_static_object_public::value_t> result_s;
    land_rtree_ptr->query(bgi::intersects(query_box), std::back_inserter(result_s));
    return result_s;
}

void load_from_dump_if_empty(sea_static_object_public::rtree_t* rtree_ptr, const char* dump_filename) {
    if (rtree_ptr->size() == 0) {
        printf("R-tree empty. Trying to create R-tree from dump file %s...\n", dump_filename);
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
            printf("Max rect R Tree size (after loaded from %s): %zu\n", dump_filename, rtree_ptr->size());
        }
        else {
            printf("[Error] Dump file %s not exist!\n", dump_filename);
        }
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
    , km_per_cell(WORLD_CIRCUMFERENCE_IN_KM / res_width) {

    load_from_dump_if_empty(land_rtree_ptr, "rtree/land_raw_xy32xy32.bin");
    load_from_dump_if_empty(water_rtree_ptr, "rtree/water_raw_xy32xy32.bin");

    xy32 from = { 14065, 2496 };
    xy32 to = { 14043, 2512 };
    astarrtree::astar_rtree_memory(water_rtree_ptr, from, to);
}

std::vector<xy32> ss::sea_static::calculate_waypoints(const xy32 & from, const xy32 & to) const {
    return astarrtree::astar_rtree_memory(water_rtree_ptr, from, to);
}

std::vector<xy32> ss::sea_static::calculate_waypoints(const sea_static_object_public::point_t & from, const sea_static_object_public::point_t & to) const {
    xy32 fromxy;
    xy32 toxy;
    fromxy.x = from.get<0>();
    fromxy.y = from.get<1>();
    toxy.x = to.get<0>();
    toxy.y = to.get<1>();
    return calculate_waypoints(fromxy, toxy);
}

bool ss::sea_static::is_water(const xy32& cell) const {
    auto cell_box = astarrtree::box_t_from_xy(cell);
    return water_rtree_ptr->qbegin(bgi::contains(cell_box)) != water_rtree_ptr->qend();
}
