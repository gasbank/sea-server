#include "precompiled.hpp"
#include "astarrtree.hpp"
#include "AStar.h"

using namespace astarrtree;
using ss::LOGI;
using ss::LOGIx;
using ss::LOGE;
using ss::LOGEx;

struct PathfindContext {
    xy32xy32 from_rect;
    xy32xy32 to_rect;
    rtree_t* rtree;
};

xy32xy32 xyxy_from_box_t(const box_t& v) {
    xy32xy32 r;
    r.xy0.x = v.min_corner().get<0>();
    r.xy0.y = v.min_corner().get<1>();
    r.xy1.x = v.max_corner().get<0>();
    r.xy1.y = v.max_corner().get<1>();
    return r;
}

box_t box_t_from_xyxy(const xy32xy32& v) {
    box_t r;
    r.min_corner().set<0>(v.xy0.x);
    r.min_corner().set<1>(v.xy0.y);
    r.max_corner().set<0>(v.xy1.x);
    r.max_corner().set<1>(v.xy1.y);
    return r;
}

void RTreePathNodeNeighbors(ASNeighborList neighbors, void *node, void *context) {
    auto n = reinterpret_cast<const xy32xy32xy32*>(node);
    rtree_t* rtree_ptr = reinterpret_cast<PathfindContext*>(context)->rtree;
    box_t query_box = box_t_from_xyxy(n->box);
    std::vector<value_t> result_s;
    rtree_ptr->query(bgi::intersects(query_box), std::back_inserter(result_s));
    for (const auto& v : result_s) {
        auto xyxy = xyxy_from_box_t(v.first);
        const xy32 enter_point{
            boost::algorithm::clamp(n->point.x, xyxy.xy0.x, xyxy.xy1.x - 1),
            boost::algorithm::clamp(n->point.y, xyxy.xy0.y, xyxy.xy1.y - 1),
        };
        xy32xy32xy32 n2 = { xyxy, enter_point };
        const int dx = enter_point.x - n->point.x;
        const int dy = enter_point.y - n->point.y;
        const float edge_cost = sqrtf(static_cast<float>(dx * dx + dy * dy));
        ASNeighborListAdd(neighbors, &n2, edge_cost);
    }
}

int RectDistance(const xy32xy32* a, const xy32xy32* b) {
    int dx = 0, dy = 0;
    // check overlapping along x axis
    const int a_and_b_x = a->xy1.x - b->xy0.x;
    const int b_and_a_x = b->xy1.x - a->xy0.x;
    if (a_and_b_x < 0) {
        dx = -a_and_b_x;
    } else if (b_and_a_x < 0) {
        dx = -b_and_a_x;
    }
    // check overlapping along y axis
    const int a_and_b_y = a->xy1.y - b->xy0.y;
    const int b_and_a_y = b->xy1.y - a->xy0.y;
    if (a_and_b_y < 0) {
        dy = -a_and_b_y;
    } else if (b_and_a_y < 0) {
        dy = -b_and_a_y;
    }
    assert(dx >= 0);
    assert(dy >= 0);
    return dx + dy;
}

float RTreePathNodeHeuristic(void *fromNode, void *toNode, void *context) {
    const auto from = reinterpret_cast<const xy32xy32xy32*>(fromNode);
    const auto to = reinterpret_cast<const xy32xy32xy32*>(toNode);

    // [1] Cell Midpoint Distance Method
    // TOO SLOW; illogical on very big cells
    const auto fromMedX = (from->box.xy1.x - from->box.xy0.x) / 2.0f;
    const auto fromMedY = (from->box.xy1.y - from->box.xy0.y) / 2.0f;
    const auto toMedX = (to->box.xy1.x - to->box.xy0.x) / 2.0f;
    const auto toMedY = (to->box.xy1.y - to->box.xy0.y) / 2.0f;
    /*
    return fabsf(fromMedX - toMedX) + fabsf(fromMedY - toMedY);*/

    // [2] Global Direction Following Method
    // Less cost if moving to the same direction... (kind of greedy)
    const auto global_from_rect = &reinterpret_cast<PathfindContext*>(context)->from_rect;
    const auto global_to_rect = &reinterpret_cast<PathfindContext*>(context)->to_rect;

    const auto dir_rect = atan2f(static_cast<float>(global_to_rect->xy0.y - global_from_rect->xy0.y),
                                 static_cast<float>(global_to_rect->xy0.x - global_from_rect->xy0.x));
    /*const float dir = atan2f(static_cast<float>(to->xy0.y - from->xy0.y),
                             static_cast<float>(to->xy0.x - from->xy0.x));*/
    const auto dir = atan2f(static_cast<float>(toMedY - fromMedY),
                            static_cast<float>(toMedX - fromMedX));
    //return fabsf(dir_rect - dir);

    // [3] Simple Manhattan Distance Method
    // INCORRECT on long cells
    //return static_cast<float>(abs(from->xy0.x - to->xy0.x) + abs(from->xy0.y - to->xy0.y));

    // [4] Rect Distance Method
    //return static_cast<float>(RectDistance(from, to) + RectDistance(to, to_rect));

    // [5] Rect Distance Method (Revised)
    const auto from_cost = RectDistance(&from->box, global_to_rect);
    const auto to_cost = RectDistance(&to->box, global_to_rect);
    const auto from_to_cost = RectDistance(&from->box, &to->box);
    //return static_cast<float>(from_to_cost != 0 ? FLT_MAX : to_cost);

    // [6] Enter Point Distance Method
    const auto enter_point_dx = from->point.x - to->point.x;
    const auto enter_point_dy = from->point.y - to->point.y;
    const auto enter_point_dist = sqrtf(static_cast<float>(enter_point_dx * enter_point_dx + enter_point_dy * enter_point_dy));
    // 'from_to_cast' is a pentalty cost nonzero penalty cost.
    // The penalty cost is non-zero only if 'fromNode' and 'toNode' are not neighboring.
    return enter_point_dist + from_to_cost;

    // [7] No cost
    //return 0;
}

int RTreePathNodeComparator(void *node1, void *node2, void *context) {
    const auto n1 = reinterpret_cast<const xy32xy32xy32*>(node1);
    const auto n1v = (static_cast<int64_t>(n1->box.xy0.y) << 32) | static_cast<int64_t>(n1->box.xy0.x);
    const auto n2 = reinterpret_cast<const xy32xy32xy32*>(node2);
    const auto n2v = (static_cast<int64_t>(n2->box.xy0.y) << 32) | static_cast<int64_t>(n2->box.xy0.x);
    const auto d = n1v - n2v;
    if (d == 0) {
        return 0;
    } else if (d > 0) {
        return 1;
    } else {
        return -1;
    }
}

float xyib_distance(const xy32ib& a, const xy32ib& b) {
    const int dx = a.p.x - b.p.x;
    const int dy = a.p.y - b.p.y;
    return sqrtf(static_cast<float>(dx * dx + dy * dy));
}

struct pixel_waypoint_search {
    xy32 from;
    xy32 to;
    ASPath cell_path;
};

void RTreePixelPathNodeNeighbors(ASNeighborList neighbors, void *node, void *context);
typedef void(*AStarNodeNeighborsCallback)(ASNeighborList neighbors, void *node, void *context);

void AddNeighborWithLog(ASNeighborList neighbors, xy32ib* n, pixel_waypoint_search* pws, int x, int y, size_t i, XYIB_ENTER_EXIT ee, AStarNodeNeighborsCallback cb) {
    xy32ib neighbor = { { x, y }, i, ee };
    if (n->p.x == x && n->p.y == y && n->ee == XEE_ENTER && ee == XEE_EXIT && n->i == i) {
        // if 'n' and 'neighbor' have the same coordinates...
        // make 'n' exit node and rerun RTreePixelPathNodeNeighbors()
        n->ee = XEE_EXIT;
        cb(neighbors, n, pws);
    } else {
        ASNeighborListAdd(neighbors, &neighbor, xyib_distance(*n, neighbor));
        LOGIx("Neighbor of (%5d,%5d)[%5d] : (%5d,%5d)[%5d]",
              n->p.x,
              n->p.y,
              n->i,
              x,
              y,
              i);
    }
}

enum RECT_RELATION {
    RR_DOWN_RIGHT,
    RR_UP_RIGHT,
    RR_UP_LEFT,
    RR_DOWN_LEFT,
    RR_DOWN,
    RR_UP,
    RR_RIGHT,
    RR_LEFT,
    RR_UNKNOWN,
};


RECT_RELATION rect_relation(const xy32xy32* n1c, const xy32xy32* n2c) {
    bool d = false, u = false, r = false, l = false;
    if (n1c->xy1.y <= n2c->xy0.y) {
        // [D]OWN
        //  n1c
        // -----
        //  n2c
        d = true;
    }
    if (n2c->xy1.y <= n1c->xy0.y) {
        // [U]P
        //  n2c
        // -----
        //  n1c
        u = true;
    }
    if (n1c->xy1.x <= n2c->xy0.x) {
        // [R]IGHT
        //      |
        //  n1c | n2c
        //      |
        r = true;
    }
    if (n2c->xy1.x <= n1c->xy0.x) {
        // [L]EFT
        //      |
        //  n2c | n1c
        //      |
        l = true;
    }
    if (d && !u && r && !l) {
        return RR_DOWN_RIGHT;
    } else if (!d && u && r && !l) {
        return RR_UP_RIGHT;
    } else if (!d && u && !r && l) {
        return RR_UP_LEFT;
    } else if (d && !u && !r && l) {
        return RR_DOWN_LEFT;
    } else if (d && !u && !r && !l) {
        return RR_DOWN;
    } else if (!d && u && !r && !l) {
        return RR_UP;
    } else if (!d && !u && r && !l) {
        return RR_RIGHT;
    } else if (!d && !u && !r && l) {
        return RR_LEFT;
    } else {
        LOGE("Unknown rect relation...");
        return RR_UNKNOWN;
    }
}

RECT_RELATION rect_neighbor_relation(const xy32xy32* n1c, const xy32xy32* n2c) {
    bool d = false, u = false, r = false, l = false;
    if (n1c->xy1.y == n2c->xy0.y) {
        // [D]OWN
        //  n1c
        // -----
        //  n2c
        d = true;
    }
    if (n2c->xy1.y == n1c->xy0.y) {
        // [U]P
        //  n2c
        // -----
        //  n1c
        u = true;
    }
    if (n1c->xy1.x == n2c->xy0.x) {
        // [R]IGHT
        //      |
        //  n1c | n2c
        //      |
        r = true;
    }
    if (n2c->xy1.x == n1c->xy0.x) {
        // [L]EFT
        //      |
        //  n2c | n1c
        //      |
        l = true;
    }
    if (d && !u && r && !l) {
        return RR_DOWN_RIGHT;
    } else if (!d && u && r && !l) {
        return RR_UP_RIGHT;
    } else if (!d && u && !r && l) {
        return RR_UP_LEFT;
    } else if (d && !u && !r && l) {
        return RR_DOWN_LEFT;
    } else if (d && !u && !r && !l) {
        return RR_DOWN;
    } else if (!d && u && !r && !l) {
        return RR_UP;
    } else if (!d && !u && r && !l) {
        return RR_RIGHT;
    } else if (!d && !u && !r && l) {
        return RR_LEFT;
    } else {
        LOGE("Unknown rect relation...");
        return RR_UNKNOWN;
    }
}


int ClampIntInclusiveExclusive(int v, int beg, int end) {
    if (beg >= end) {
        abort();
    }
    if (v < beg) {
        return beg;
    } else if (v >= end) {
        return end - 1;
    } else {
        return v;
    }
}

void AddNeighborByRectRelation(xy32xy32* n1c,
                               xy32xy32* n2c,
                               const ASNeighborList& neighbors,
                               xy32ib* n,
                               pixel_waypoint_search* pws,
                               const size_t& next_i,
                               XYIB_ENTER_EXIT next_ee,
                               bool add_nearest_only,
                               AStarNodeNeighborsCallback cb) {
    switch (rect_neighbor_relation(n1c, n2c)) {
    case RR_DOWN_RIGHT:
        AddNeighborWithLog(neighbors, n, pws, n2c->xy0.x, n2c->xy0.y, next_i, next_ee, cb);
        break;
    case RR_UP_RIGHT:
        AddNeighborWithLog(neighbors, n, pws, n2c->xy0.x, n2c->xy1.y - 1, next_i, next_ee, cb);
        break;
    case RR_UP_LEFT:
        AddNeighborWithLog(neighbors, n, pws, n2c->xy1.x - 1, n2c->xy1.y - 1, next_i, next_ee, cb);
        break;
    case RR_DOWN_LEFT:
        AddNeighborWithLog(neighbors, n, pws, n2c->xy1.x - 1, n2c->xy0.y, next_i, next_ee, cb);
        break;
    case RR_DOWN:
    {
        const int xbeg = std::max(n1c->xy0.x, n2c->xy0.x);
        const int xend = std::min(n1c->xy1.x, n2c->xy1.x);
        if (add_nearest_only) {
            int x = ClampIntInclusiveExclusive(n->p.x, xbeg, xend);
            AddNeighborWithLog(neighbors, n, pws, x, n2c->xy0.y, next_i, next_ee, cb);
        } else {
            for (int x = xbeg; x < xend; x++) {
                AddNeighborWithLog(neighbors, n, pws, x, n2c->xy0.y, next_i, next_ee, cb);
            }
        }
        break;
    }
    case RR_UP:
    {
        const int xbeg = std::max(n1c->xy0.x, n2c->xy0.x);
        const int xend = std::min(n1c->xy1.x, n2c->xy1.x);
        if (add_nearest_only) {
            int x = ClampIntInclusiveExclusive(n->p.x, xbeg, xend);
            AddNeighborWithLog(neighbors, n, pws, x, n2c->xy1.y - 1, next_i, next_ee, cb);
        } else {
            for (int x = xbeg; x < xend; x++) {
                AddNeighborWithLog(neighbors, n, pws, x, n2c->xy1.y - 1, next_i, next_ee, cb);
            }
        }
        break;
    }
    case RR_RIGHT:
    {
        const int ybeg = std::max(n1c->xy0.y, n2c->xy0.y);
        const int yend = std::min(n1c->xy1.y, n2c->xy1.y);
        if (add_nearest_only) {
            int y = ClampIntInclusiveExclusive(n->p.y, ybeg, yend);
            AddNeighborWithLog(neighbors, n, pws, n2c->xy0.x, y, next_i, next_ee, cb);
        } else {
            for (int y = ybeg; y < yend; y++) {
                AddNeighborWithLog(neighbors, n, pws, n2c->xy0.x, y, next_i, next_ee, cb);
            }
        }
        break;
    }

    case RR_LEFT:
    {
        const int ybeg = std::max(n1c->xy0.y, n2c->xy0.y);
        const int yend = std::min(n1c->xy1.y, n2c->xy1.y);
        if (add_nearest_only) {
            int y = ClampIntInclusiveExclusive(n->p.y, ybeg, yend);
            AddNeighborWithLog(neighbors, n, pws, n2c->xy1.x - 1, y, next_i, next_ee, cb);
        } else {
            for (int y = ybeg; y < yend; y++) {
                AddNeighborWithLog(neighbors, n, pws, n2c->xy1.x - 1, y, next_i, next_ee, cb);
            }
        }
        break;
    }
    case RR_UNKNOWN:
    default:
        LOGE("Logic error for finding neighboring pixels...");
        abort();
        break;
    }
}

void RTreePixelPathNodeNeighbors(ASNeighborList neighbors, void *node, void *context) {
    xy32ib* n = reinterpret_cast<xy32ib*>(node);
    pixel_waypoint_search* pws = reinterpret_cast<pixel_waypoint_search*>(context);
    ASPath cell_path = pws->cell_path;
    size_t cell_path_count = ASPathGetCount(cell_path);
    if (n->p.x == pws->to.x && n->p.y == pws->to.y) {
        // 'n' equals to 'to': reached endpoint (to-pixel)
        // no neighbors on endpoint
        return;
    } else if (n->i == cell_path_count - 1 && n->ee == XEE_ENTER) {
        // all last cells' pixels (except endpoint pixel) always have the only neighbor: endpoint pixel
        AddNeighborWithLog(neighbors, n, pws, pws->to.x, pws->to.y, cell_path_count - 1, XEE_EXIT, RTreePixelPathNodeNeighbors);
        return;
    }
    xy32xy32* n1c;
    xy32xy32* n2c;
    XYIB_ENTER_EXIT next_ee = XEE_ENTER;
    size_t next_i = 0;
    if (n->ee == XEE_ENTER) {
        // Get exit nodes at the same cell node
        n1c = reinterpret_cast<xy32xy32*>(ASPathGetNode(cell_path, n->i + 1)); // Next Cell node
        n2c = reinterpret_cast<xy32xy32*>(ASPathGetNode(cell_path, n->i + 0)); // Cell node containing 'n'
        next_ee = XEE_EXIT;
        next_i = n->i; // the same cell node
    } else if (n->ee == XEE_EXIT) {
        // Get enter nodes at the next cell node
        n1c = reinterpret_cast<xy32xy32*>(ASPathGetNode(cell_path, n->i + 0)); // Cell node containing 'n'
        n2c = reinterpret_cast<xy32xy32*>(ASPathGetNode(cell_path, n->i + 1)); // Next Cell node
        next_ee = XEE_ENTER;
        next_i = n->i + 1; // the next cell node
    } else {
        LOGE("Corrupted ee");
        abort();
    }
    AddNeighborByRectRelation(n1c,
                              n2c,
                              neighbors,
                              n,
                              pws,
                              next_i,
                              next_ee,
                              false,
                              RTreePixelPathNodeNeighbors);
}

void RTreePixelPathNodeNeighborsSuboptimal(ASNeighborList neighbors, void *node, void *context) {
    xy32ib* n = reinterpret_cast<xy32ib*>(node);
    pixel_waypoint_search* pws = reinterpret_cast<pixel_waypoint_search*>(context);
    ASPath cell_path = pws->cell_path;
    size_t cell_path_count = ASPathGetCount(cell_path);
    if (n->p.x == pws->to.x && n->p.y == pws->to.y) {
        // 'n' equals to 'to': reached endpoint (to-pixel)
        // no neighbors on endpoint
        return;
    } else if (n->i == cell_path_count - 1 && n->ee == XEE_ENTER) {
        // all last cells' pixels (except endpoint pixel) always have the only neighbor: endpoint pixel
        AddNeighborWithLog(neighbors, n, pws, pws->to.x, pws->to.y, cell_path_count - 1, XEE_EXIT, RTreePixelPathNodeNeighborsSuboptimal);
        return;
    }
    xy32xy32* n1c;
    xy32xy32* n2c;
    XYIB_ENTER_EXIT next_ee = XEE_ENTER;
    size_t next_i = 0;
    if (n->ee == XEE_ENTER) {
        // Get exit nodes at the same cell node
        n1c = reinterpret_cast<xy32xy32*>(ASPathGetNode(cell_path, n->i + 1)); // Next Cell node
        n2c = reinterpret_cast<xy32xy32*>(ASPathGetNode(cell_path, n->i + 0)); // Cell node containing 'n'
        next_ee = XEE_EXIT;
        next_i = n->i; // the same cell node
    } else if (n->ee == XEE_EXIT) {
        // Get enter nodes at the next cell node
        n1c = reinterpret_cast<xy32xy32*>(ASPathGetNode(cell_path, n->i + 0)); // Cell node containing 'n'
        n2c = reinterpret_cast<xy32xy32*>(ASPathGetNode(cell_path, n->i + 1)); // Next Cell node
        next_ee = XEE_ENTER;
        next_i = n->i + 1; // the next cell node
    } else {
        LOGE("Corrupted ee");
        abort();
    }
    const bool add_nearest_only = !(n->i == 0 && n->ee == XEE_ENTER);
    AddNeighborByRectRelation(n1c,
                              n2c,
                              neighbors,
                              n,
                              pws,
                              next_i,
                              next_ee,
                              add_nearest_only,
                              RTreePixelPathNodeNeighborsSuboptimal);
}

float RTreePixelPathNodeHeuristic(void *fromNode, void *toNode, void *context) {
    xy32ib* from = reinterpret_cast<xy32ib*>(fromNode);
    xy32ib* to = reinterpret_cast<xy32ib*>(toNode);
    return xyib_distance(*from, *to);
    //return static_cast<float>(abs(from->p.x - to->p.x) + abs(from->p.y - to->p.y));
}

int RTreePixelPathNodeComparator(void *node1, void *node2, void *context) {
    xy32ib* n1 = reinterpret_cast<xy32ib*>(node1);
    int64_t n1v = static_cast<int64_t>(n1->p.y) << 32 | n1->p.x;
    xy32ib* n2 = reinterpret_cast<xy32ib*>(node2);
    int64_t n2v = static_cast<int64_t>(n2->p.y) << 32 | n2->p.x;
    int64_t d = n1v - n2v;
    if (d == 0) {
        return 0;
    } else if (d > 0) {
        return 1;
    } else {
        return -1;
    }
}

box_t astarrtree::box_t_from_xy(xy32 v) {
    return box_t(point_t(v.x, v.y), point_t(v.x + 1, v.y + 1));
}

xy32xy32 xyxy_from_xy(xy32 v) {
    return xy32xy32{ { v.x, v.y },{ v.x + 1, v.y + 1 } };
}

std::vector<xy32> calculate_pixel_waypoints(xy32 from, xy32 to, ASPath cell_path) {
    std::vector<xy32> waypoints;
    ASPathNodeSource PathNodeSource =
    {
        sizeof(xy32ib),
        RTreePixelPathNodeNeighborsSuboptimal,
        RTreePixelPathNodeHeuristic,
        NULL,
        RTreePixelPathNodeComparator
    };
    size_t cell_path_count = ASPathGetCount(cell_path);
    if (cell_path_count == 0) {
        LOGE("calculate_pixel_waypoints: cell_path_count is 0.");
        abort();
    }
    xy32ib from_rect = { from, 0, XEE_ENTER };
    xy32ib to_rect = { to, cell_path_count - 1, XEE_EXIT };
    pixel_waypoint_search pws = { from, to, cell_path };
    ASPath pixel_path = ASPathCreate(&PathNodeSource, &pws, &from_rect, &to_rect);
    size_t pixel_path_count = ASPathGetCount(pixel_path);
    if (pixel_path_count > 0) {
        LOGI("Path Count: %1%", pixel_path_count);
        float pixel_path_cost = ASPathGetCost(pixel_path);
        LOGI("Path Cost: %f", pixel_path_cost);
        /*if (pixel_path_cost < 6000)*/
        {
            for (size_t i = 0; i < pixel_path_count; i++) {
                xy32ib* pixel_node = reinterpret_cast<xy32ib*>(ASPathGetNode(pixel_path, i));
                LOGIx("Pixel Path %1%: (%2%, %3%) [Cell index=%4%]",
                      i,
                      pixel_node->p.x,
                      pixel_node->p.y,
                      pixel_node->i);
                waypoints.push_back(pixel_node->p);
            }
        }
    } else {
        LOGE("No pixel waypoints found.");
    }
    ASPathDestroy(pixel_path);
    return waypoints;
}

void astarrtree::astar_rtree(const char* rtree_filename, size_t output_max_size, xy32 from, xy32 to) {
    bi::managed_mapped_file file(bi::open_or_create, rtree_filename, output_max_size);
    allocator_t alloc(file.get_segment_manager());
    rtree_t* rtree_ptr = file.find_or_construct<rtree_t>("rtree")(params_t(), indexable_t(), equal_to_t(), alloc);
    astar_rtree_memory(rtree_ptr, from, to);
}

bool astarrtree::find_nearest_point_if_empty(rtree_t* rtree_ptr, xy32& from, box_t& from_box, std::vector<value_t>& from_result_s) {
    if (from_result_s.size() == 0) {
        auto nearest_it = rtree_ptr->qbegin(bgi::nearest(from_box, 1));
        if (nearest_it == rtree_ptr->qend()) {
            LOGE("Empty result from nearest query...");
            abort();
        }
        auto nearest_xyxy = xyxy_from_box_t(nearest_it->first);
        auto from_xyxy = xyxy_from_xy(from);
        switch (rect_relation(&nearest_xyxy, &from_xyxy)) {
        case RR_DOWN_RIGHT:
            from = { nearest_xyxy.xy1.x - 1, nearest_xyxy.xy1.y - 1 };
            from_box = box_t_from_xy(from);
            from_result_s.push_back(*nearest_it);
            break;
        case RR_UP_RIGHT:
            from = { nearest_xyxy.xy1.x - 1, nearest_xyxy.xy0.y };
            from_box = box_t_from_xy(from);
            from_result_s.push_back(*nearest_it);
            break;
        case RR_UP_LEFT:
            from = { nearest_xyxy.xy0.x, nearest_xyxy.xy0.y };
            from_box = box_t_from_xy(from);
            from_result_s.push_back(*nearest_it);
            break;
        case RR_DOWN_LEFT:
            from = { nearest_xyxy.xy0.x, nearest_xyxy.xy1.y - 1 };
            from_box = box_t_from_xy(from);
            from_result_s.push_back(*nearest_it);
            break;
        case RR_DOWN:
            from = { from.x, nearest_xyxy.xy1.y - 1 };
            from_box = box_t_from_xy(from);
            from_result_s.push_back(*nearest_it);
            break;
        case RR_UP:
            from = { from.x, nearest_xyxy.xy0.y };
            from_box = box_t_from_xy(from);
            from_result_s.push_back(*nearest_it);
            break;
        case RR_RIGHT:
            from = { nearest_xyxy.xy1.x - 1, from.y };
            from_box = box_t_from_xy(from);
            from_result_s.push_back(*nearest_it);
            break;
        case RR_LEFT:
            from = { nearest_xyxy.xy0.x, from.y };
            from_box = box_t_from_xy(from);
            from_result_s.push_back(*nearest_it);
            break;
        case RR_UNKNOWN:
        default:
            LOGE("rect relation unknown...");
            abort();
            break;
        }
        return true;
    } else {
        return false;
    }
}

std::vector<xy32> astarrtree::astar_rtree_memory(rtree_t* rtree_ptr, xy32 from, xy32 to) {
    float distance = static_cast<float>(abs(from.x - to.x) + abs(from.y - to.y));
    ss::LOGI("Pathfinding from (%1%,%2%) -> (%3%,%4%) [Manhattan distance = %5%]",
             from.x,
             from.y,
             to.x,
             to.y,
             distance);

    std::vector<xy32> waypoints;
    ss::LOGI("R Tree size: %1%", rtree_ptr->size());
    if (rtree_ptr->size() == 0) {
        return waypoints;
    }

    auto from_box = box_t_from_xy(from);
    std::vector<value_t> from_result_s;
    rtree_ptr->query(bgi::contains(from_box), std::back_inserter(from_result_s));
    if (astarrtree::find_nearest_point_if_empty(rtree_ptr, from, from_box, from_result_s)) {
        ss::LOGI("  'From' point changed to (%1%,%2%)",
                 from.x,
                 from.y);
    }

    auto to_box = box_t_from_xy(to);
    std::vector<value_t> to_result_s;
    rtree_ptr->query(bgi::contains(to_box), std::back_inserter(to_result_s));
    if (astarrtree::find_nearest_point_if_empty(rtree_ptr, to, to_box, to_result_s)) {
        ss::LOGI("  'To' point changed to (%1%,%2%)",
                 to.x,
                 to.y);
    }

    if (from_result_s.size() == 1 && to_result_s.size() == 1) {
        // Phase 1 - R Tree rectangular node searching
        ASPathNodeSource PathNodeSource =
        {
            sizeof(xy32xy32xy32),
            RTreePathNodeNeighbors,
            RTreePathNodeHeuristic,
            NULL,
            RTreePathNodeComparator
        };
        xy32xy32xy32 from_rect = { xyxy_from_box_t(from_result_s[0].first), from };
        xy32xy32xy32 to_rect = { xyxy_from_box_t(to_result_s[0].first), to };
        PathfindContext context{
            { { from.x,from.y },{ from.x + 1, from.y + 1 } },
            { { to.x,to.y },{ to.x + 1, to.y + 1 } },
            rtree_ptr
        };
        ASPath path = ASPathCreate(&PathNodeSource, &context, &from_rect, &to_rect);
        size_t pathCount = ASPathGetCount(path);
        if (pathCount > 0) {
            LOGI("Cell Path Count: %1%", pathCount);
            float pathCost = ASPathGetCost(path);
            LOGI("Cell Path Cost: %f", pathCost);
            /*if (pathCost < 6000)*/
            {
                for (size_t i = 0; i < pathCount; i++) {
                    xy32xy32* node = reinterpret_cast<xy32xy32*>(ASPathGetNode(path, i));
                    LOGIx("Cell Path %1%: (%2%, %3%)-(%4%, %5%) [%6% x %7% = %8%]",
                          i,
                          node->xy0.x,
                          node->xy0.y,
                          node->xy1.x,
                          node->xy1.y,
                          node->xy1.x - node->xy0.x,
                          node->xy1.y - node->xy0.y,
                          (node->xy1.x - node->xy0.x) * (node->xy1.y - node->xy0.y));
                }
            }
            // Phase 2 - per-pixel node searching
            waypoints = calculate_pixel_waypoints(from, to, path);
        } else {
            LOGE("No path found.");
        }
        ASPathDestroy(path);
    } else {
        LOGE("From-node and/or to-node error.");
    }
    return waypoints;
}
