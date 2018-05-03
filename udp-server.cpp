#include "precompiled.hpp"
#include "udp-server.hpp"
#include "sea.hpp"
#include "sea_static.hpp"
#include "seaport.hpp"
#include "lz4.h"
#include "route.hpp"
#include "xy.hpp"
#include "packet.h"
#include "region.hpp"

using namespace ss;

const auto update_interval = boost::posix_time::milliseconds(75);
//const auto update_interval = boost::posix_time::milliseconds(250);

udp_server::udp_server(boost::asio::io_service & io_service,
                       std::shared_ptr<sea> sea,
                       std::shared_ptr<sea_static> sea_static,
                       std::shared_ptr<seaport> seaport,
                       std::shared_ptr<region> region)
    : socket_(io_service, udp::endpoint(udp::v4(), 3100))
    , timer_(io_service, update_interval)
    , sea_(sea)
    , sea_static_(sea_static)
    , seaport_(seaport)
    , region_(region)
    , tick_seq_(0) {

    //make_test_route();

    start_receive();
    timer_.async_wait(boost::bind(&udp_server::update, this));
}

void udp_server::make_test_route() {
    auto spawn_point = seaport_->get_seaport_point("Busan");
    auto spawn_point_x = static_cast<float>(spawn_point.get<0>());
    auto spawn_point_y = static_cast<float>(spawn_point.get<1>());
    auto id1 = sea_->spawn("Test A", 1, spawn_point_x, spawn_point_y, 1, 1);
    auto id2 = sea_->spawn("Test B", 2, spawn_point_x, spawn_point_y, 1, 1);
    auto id3 = sea_->spawn("Test C", 3, spawn_point_x, spawn_point_y, 1, 1);
    auto id4 = sea_->spawn("Test D", 4, spawn_point_x, spawn_point_y, 1, 1);
    auto id5 = sea_->spawn("Test E", 5, spawn_point_x, spawn_point_y, 1, 1);
    auto id6 = sea_->spawn("Test F", 6, spawn_point_x, spawn_point_y, 1, 1);
    auto id7 = sea_->spawn("Test G", 7, spawn_point_x, spawn_point_y, 1, 1);
    auto id8 = sea_->spawn("Test H", 8, spawn_point_x, spawn_point_y, 1, 1);

    route_map_[id1] = create_route({
        "Onsan/Ulsan",
        "Busan",
        "Busan New Port",
        "Anjeong",
        "Tongyeong" });

    route_map_[id2] = create_route({
        "Busan",
        "South Busan" });

    route_map_[id3] = create_route({
        "Tongyeong",
        "Samcheonpo/Sacheon" });

    route_map_[id4] = create_route({
        "Gwangyang",
        "Yeosu" });

    route_map_[id5] = create_route({
        "Yokohama",
        "Nokdongsin" });

    // Too slow in 'Debug' mode...
    //route_map_[id6] = create_route({
    //    "Onsan/Ulsan",
    //    "Yokohama" });

    // Too slow even in 'Release' mode...
    //route_map_[id7] = create_route({
    //    "Pilottown",
    //    "Yokohama" });

    //route_map_[id8] = create_route({
    //    "Yokohama",
    //    "Jurong/Singapore" });

    LOGI("Route setup completed.");
}

bool udp_server::set_route(int id, int seaport_id1, int seaport_id2) {
    auto route = create_route_id({ seaport_id1, seaport_id2 });
    if (route) {
        route_map_[id] = route;
        return true;
    }
    return false;
}

void udp_server::update() {
    tick_seq_++;

    timer_.expires_at(timer_.expires_at() + update_interval);
    timer_.async_wait(boost::bind(&udp_server::update, this));

    float delta_time = update_interval.total_milliseconds() / 1000.0f;
    sea_->update(delta_time);
    std::vector<int> remove_keys;
    for (auto v : route_map_) {
        if (sea_->update_route(delta_time, v.first, v.second) == false) {
            // no more valid 'v'
            remove_keys.push_back(v.first);
        }
    }
    for (auto v : remove_keys) {
        route_map_.erase(v);
    }
}

void udp_server::start_receive() {
    socket_.async_receive_from(boost::asio::buffer(recv_buffer_),
                               remote_endpoint_,
                               boost::bind(&udp_server::handle_receive,
                                           this,
                                           boost::asio::placeholders::error,
                                           boost::asio::placeholders::bytes_transferred));
}

void udp_server::handle_send(const boost::system::error_code & error, std::size_t bytes_transferred) {
    if (error) {
        LOGE("ERROR: %s", error);
    } else {
        LOGIx("%1% bytes sent.", bytes_transferred);
    }
}

void udp_server::send_full_state(float lng, float lat, float ex_lng, float ex_lat, int view_scale) {
    std::vector<sea_object_public> sop_list;
    sea_->query_near_lng_lat_to_packet(lng, lat, ex_lng * view_scale, ex_lat * view_scale, sop_list);

    boost::shared_ptr<LWPTTLFULLSTATE> reply(new LWPTTLFULLSTATE);
    memset(reply.get(), 0, sizeof(LWPTTLFULLSTATE));
    reply->type = 109; // LPGP_LWPTTLFULLSTATE
    size_t reply_obj_index = 0;
    for (sea_object_public const& v : sop_list) {
        reply->obj[reply_obj_index].fx0 = v.fx;
        reply->obj[reply_obj_index].fy0 = v.fy;
        reply->obj[reply_obj_index].fx1 = v.fx + v.fw;
        reply->obj[reply_obj_index].fy1 = v.fy + v.fh;
        reply->obj[reply_obj_index].fvx = v.fvx;
        reply->obj[reply_obj_index].fvy = v.fvy;
        reply->obj[reply_obj_index].id = v.id;
        reply->obj[reply_obj_index].type = v.type;
        strcpy(reply->obj[reply_obj_index].guid, v.guid);
        auto it = route_map_.find(v.id);
        if (it != route_map_.end() && it->second) {
            reply->obj[reply_obj_index].route_left = it->second->get_left();
        } else {
            reply->obj[reply_obj_index].route_left = 0;
        }

        reply_obj_index++;
        if (reply_obj_index >= boost::size(reply->obj)) {
            break;
        }
    }
    reply->count = static_cast<int>(reply_obj_index);
    LOGIx("Querying (%1%,%2%) extent (%3%,%4%) => %5% hit(s).", lng, lat, ex_lng, ex_lat, reply_obj_index);
    char compressed[1500];
    int compressed_size = LZ4_compress_default((char*)reply.get(), compressed, sizeof(LWPTTLFULLSTATE), static_cast<int>(boost::size(compressed)));
    if (compressed_size > 0) {
        socket_.async_send_to(boost::asio::buffer(compressed, compressed_size),
                              remote_endpoint_,
                              boost::bind(&udp_server::handle_send,
                                          this,
                                          boost::asio::placeholders::error,
                                          boost::asio::placeholders::bytes_transferred));
    } else {
        LOGE("%1%: LZ4_compress_default() error! - %2%",
             __func__,
             compressed_size);
    }
}

static signed char aligned_scaled_offset(const int cell_index, const int aligned_cell_index, const int view_scale, const int view_scale_msb_index, const bool clamp, int lo, int hi) {
    const auto cell_index_scaled = cell_index & ~(view_scale - 1);
    const auto offset = cell_index_scaled - aligned_cell_index;
    try {
        return boost::numeric_cast<signed char>((clamp ? boost::algorithm::clamp(offset, lo, hi) : offset) >> view_scale_msb_index);
    } catch (const boost::numeric::negative_overflow& o) {
        LOGE(o.what());
    }
    return 0;
}

void udp_server::send_land_cell(float lng, float lat, float ex_lng, float ex_lat, int view_scale) {
    const auto xc0 = sea_static_->lng_to_xc(lng);
    const auto yc0 = sea_static_->lat_to_yc(lat);
    const auto xc0_aligned = aligned_chunk_index(xc0, view_scale, static_cast<int>(ex_lng));
    const auto yc0_aligned = aligned_chunk_index(yc0, view_scale, static_cast<int>(ex_lat));
    send_land_cell_aligned(xc0_aligned,
                           yc0_aligned,
                           ex_lng,
                           ex_lat,
                           view_scale);
}

void udp_server::send_land_cell_aligned(int xc0_aligned, int yc0_aligned, float ex_lng, float ex_lat, int view_scale) {
    const auto half_lng_cell_pixel_extent = boost::math::iround(ex_lng / 2.0f * view_scale);
    const auto half_lat_cell_pixel_extent = boost::math::iround(ex_lat / 2.0f * view_scale);
    auto sop_list = sea_static_->query_near_to_packet(xc0_aligned,
                                                      yc0_aligned,
                                                      ex_lng * view_scale,
                                                      ex_lat * view_scale);
    //auto sop_list = sea_static_->query_near_to_packet(xc0_aligned, yc0_aligned, xc1, yc1);


    const auto xclo = -half_lng_cell_pixel_extent;
    const auto xchi = +half_lng_cell_pixel_extent;
    const auto yclo = -half_lat_cell_pixel_extent;
    const auto ychi = +half_lat_cell_pixel_extent;
    boost::shared_ptr<LWPTTLSTATICSTATE2> reply(new LWPTTLSTATICSTATE2);
    memset(reply.get(), 0, sizeof(LWPTTLSTATICSTATE2));
    reply->type = 115; // LPGP_LWPTTLSTATICSTATE2
    reply->ts = 1;
    reply->xc0 = xc0_aligned;
    reply->yc0 = yc0_aligned;
    reply->view_scale = view_scale;
    size_t reply_obj_index = 0;
    size_t reply_obj_dropped_count = 0;
    const int view_scale_msb_index = msb_index(view_scale);
    for (const auto& v : sop_list) {
        // x-axis

        //const auto vx0 = v.x0 &~(view_scale - 1);
        //const auto vx1 = v.x1 &~(view_scale - 1);
        const auto x_scaled_offset_0 = aligned_scaled_offset(v.x0, xc0_aligned, view_scale, view_scale_msb_index, true, xclo, xchi);// boost::numeric_cast<signed char>(boost::algorithm::clamp(vx0 - xc0_aligned, xclo, xchi) >> view_scale_msb_index);
        const auto x_scaled_offset_1 = aligned_scaled_offset(v.x1, xc0_aligned, view_scale, view_scale_msb_index, true, xclo, xchi);// boost::numeric_cast<signed char>(boost::algorithm::clamp(vx1 - xc0_aligned, xclo, xchi) >> view_scale_msb_index);
        // skip degenerated one
        if (x_scaled_offset_0 >= x_scaled_offset_1) {
            continue;
        }
        // y-axis
        //const auto vy0 = v.y0 &~(view_scale - 1);
        //const auto vy1 = v.y1 &~(view_scale - 1);
        const auto y_scaled_offset_0 = aligned_scaled_offset(v.y0, yc0_aligned, view_scale, view_scale_msb_index, true, yclo, ychi); //boost::numeric_cast<signed char>(boost::algorithm::clamp(vy0 - yc0_aligned, yclo, ychi) >> view_scale_msb_index);
        const auto y_scaled_offset_1 = aligned_scaled_offset(v.y1, yc0_aligned, view_scale, view_scale_msb_index, true, yclo, ychi); //boost::numeric_cast<signed char>(boost::algorithm::clamp(vy1 - yc0_aligned, yclo, ychi) >> view_scale_msb_index);
        // skip degenerated one
        if (y_scaled_offset_0 >= y_scaled_offset_1) {
            continue;
        }
        if (reply_obj_index < boost::size(reply->obj)) {
            reply->obj[reply_obj_index].x_scaled_offset_0 = x_scaled_offset_0;
            reply->obj[reply_obj_index].y_scaled_offset_0 = y_scaled_offset_0;
            reply->obj[reply_obj_index].x_scaled_offset_1 = x_scaled_offset_1;
            reply->obj[reply_obj_index].y_scaled_offset_1 = y_scaled_offset_1;
            reply_obj_index++;
        } else {
            reply_obj_dropped_count++;
        }
    }
    reply->count = static_cast<int>(reply_obj_index);
    LOGIx("Querying (%1%,%2%) extent (%3%,%4%) => %5% hit(s).", xc0_aligned, yc0_aligned, ex_lng * view_scale, ex_lng * view_scale, reply_obj_index);
    char compressed[1500];
    int compressed_size = LZ4_compress_default((char*)reply.get(), compressed, sizeof(LWPTTLSTATICSTATE2), static_cast<int>(boost::size(compressed)));
    if (compressed_size > 0) {
        boost::crc_32_type result;
        result.process_bytes(compressed, compressed_size);
        auto crc = result.checksum();
        LOGIx("CRC: %1%", crc);

        socket_.async_send_to(boost::asio::buffer(compressed, compressed_size),
                              remote_endpoint_,
                              boost::bind(&udp_server::handle_send,
                                          this,
                                          boost::asio::placeholders::error,
                                          boost::asio::placeholders::bytes_transferred));
    } else {
        LOGE("%1%: LZ4_compress_default() error! - %2%",
             __func__,
             compressed_size);
    }
    if (reply_obj_dropped_count) {
        LOGE("%1%: %2% cells dropped. (max: %3%) Compressed size is %4% bytes.",
             __func__,
             reply_obj_dropped_count,
             boost::size(reply->obj),
             compressed_size);
    }
}

void udp_server::send_track_object_coords(int track_object_id, int track_object_ship_id) {
    sea_object* obj = nullptr;
    if (track_object_id && track_object_ship_id) {
        LOGE("track_object_id and track_object_ship_id both set. tracking ignored");
        return;
    } else if (track_object_id) {
        obj = sea_->get_object(track_object_id);
    } else if (track_object_ship_id) {
        obj = sea_->get_object_by_type(track_object_ship_id);
    }
    if (!obj) {
        LOGE("Tracking object cannot be found. (track_object_id=%1%, track_object_ship_id=%2%)",
             track_object_id,
             track_object_ship_id);
    }
    boost::shared_ptr<LWPTTLTRACKCOORDS> reply(new LWPTTLTRACKCOORDS);
    memset(reply.get(), 0, sizeof(LWPTTLTRACKCOORDS));
    reply->type = 113; // LPGP_LWPTTLTRACKCOORDS
    reply->id = obj ? (track_object_id ? track_object_id : track_object_ship_id ? track_object_ship_id : 0) : 0;
    if (obj) {
        obj->get_xy(reply->x, reply->y);
    }
    char compressed[1500];
    int compressed_size = LZ4_compress_default((char*)reply.get(), compressed, sizeof(LWPTTLTRACKCOORDS), static_cast<int>(boost::size(compressed)));
    if (compressed_size > 0) {
        socket_.async_send_to(boost::asio::buffer(compressed, compressed_size),
                              remote_endpoint_,
                              boost::bind(&udp_server::handle_send,
                                          this,
                                          boost::asio::placeholders::error,
                                          boost::asio::placeholders::bytes_transferred));
    } else {
        LOGE("%1%: LZ4_compress_default() error! - %2%",
             __func__,
             compressed_size);
    }
}

void udp_server::send_waypoints(int ship_id) {
    auto r = find_route_map_by_ship_id(ship_id);
    if (!r) {
        LOGE("Ship id %1%'s route cannot be found.", ship_id);
        return;
    }
    boost::shared_ptr<LWPTTLWAYPOINTS> reply(new LWPTTLWAYPOINTS);
    memset(reply.get(), 0, sizeof(LWPTTLWAYPOINTS));
    reply->type = 117; // LPGP_LWPTTLWAYPOINTS
    reply->ship_id = ship_id;
    auto waypoints = r->clone_waypoints();
    auto copy_len = std::min(waypoints.size(), boost::size(reply->waypoints));
    reply->count = boost::numeric_cast<int>(copy_len);
    for (size_t i = 0; i < copy_len; i++) {
        reply->waypoints[i].x = waypoints[i].x;
        reply->waypoints[i].y = waypoints[i].y;
    }
    // send
    char compressed[1500 * 4];
    int compressed_size = LZ4_compress_default((char*)reply.get(), compressed, sizeof(LWPTTLWAYPOINTS), static_cast<int>(boost::size(compressed)));
    if (compressed_size > 0) {
        socket_.async_send_to(boost::asio::buffer(compressed, compressed_size),
                              remote_endpoint_,
                              boost::bind(&udp_server::handle_send,
                                          this,
                                          boost::asio::placeholders::error,
                                          boost::asio::placeholders::bytes_transferred));
    } else {
        LOGE("%1%: LZ4_compress_default() error! - %2%",
             __func__,
             compressed_size);
    }
}

void udp_server::send_seaport(float lng, float lat, float ex_lng, float ex_lat, int view_scale) {
    const auto xc0 = sea_static_->lng_to_xc(lng);
    const auto yc0 = sea_static_->lat_to_yc(lat);
    const auto xc0_aligned = aligned_chunk_index(xc0, view_scale, static_cast<int>(ex_lng));
    const auto yc0_aligned = aligned_chunk_index(yc0, view_scale, static_cast<int>(ex_lat));
    send_seaport_cell_aligned(xc0_aligned,
                              yc0_aligned,
                              ex_lng,
                              ex_lat,
                              view_scale);
}

void udp_server::send_seaport_cell_aligned(int xc0_aligned, int yc0_aligned, float ex_lng, float ex_lat, int view_scale) {
    const auto half_lng_cell_pixel_extent = boost::math::iround(ex_lng / 2.0f * view_scale);
    const auto half_lat_cell_pixel_extent = boost::math::iround(ex_lat / 2.0f * view_scale);
    auto sop_list = seaport_->query_near_to_packet(xc0_aligned,
                                                   yc0_aligned,
                                                   ex_lng * view_scale,
                                                   ex_lat * view_scale);
    boost::shared_ptr<LWPTTLSEAPORTSTATE> reply(new LWPTTLSEAPORTSTATE);
    memset(reply.get(), 0, sizeof(LWPTTLSEAPORTSTATE));
    reply->type = 112; // LPGP_LWPTTLSEAPORTSTATE
    reply->ts = seaport_->query_ts(xc0_aligned, yc0_aligned, view_scale);
    reply->xc0 = xc0_aligned;
    reply->yc0 = yc0_aligned;
    reply->view_scale = view_scale;
    size_t reply_obj_index = 0;
    const int view_scale_msb_index = msb_index(view_scale);
    for (seaport_object_public const& v : sop_list) {
        reply->obj[reply_obj_index].x_scaled_offset_0 = aligned_scaled_offset(v.x0, xc0_aligned, view_scale, view_scale_msb_index, false, 0, 0);
        reply->obj[reply_obj_index].y_scaled_offset_0 = aligned_scaled_offset(v.y0, yc0_aligned, view_scale, view_scale_msb_index, false, 0, 0);
        if (view_scale < 16) {
            //strcpy(reply->obj[reply_obj_index].name, seaport_->get_seaport_name(v.id));
        }
        reply_obj_index++;
        if (reply_obj_index >= boost::size(reply->obj)) {
            break;
        }
    }
    reply->count = static_cast<int>(reply_obj_index);
    char compressed[1500];
    int compressed_size = LZ4_compress_default((char*)reply.get(), compressed, sizeof(LWPTTLSEAPORTSTATE), static_cast<int>(boost::size(compressed)));
    if (compressed_size > 0) {
        socket_.async_send_to(boost::asio::buffer(compressed, compressed_size),
                              remote_endpoint_,
                              boost::bind(&udp_server::handle_send,
                                          this,
                                          boost::asio::placeholders::error,
                                          boost::asio::placeholders::bytes_transferred));
    } else {
        LOGE("%1%: LZ4_compress_default() error! - %2%",
             __func__,
             compressed_size);
    }
}

void udp_server::send_seaarea(float lng, float lat) {
    std::string area_name;
    region_->query_tree(lng, lat, area_name);

    boost::shared_ptr<LWPTTLSEAAREA> reply(new LWPTTLSEAAREA);
    memset(reply.get(), 0, sizeof(LWPTTLSEAAREA));
    reply->type = 114; // LPGP_LWPTTLSEAAREA
    strcpy(reply->name, area_name.c_str());
    char compressed[1500];
    int compressed_size = LZ4_compress_default((char*)reply.get(), compressed, sizeof(LWPTTLSEAAREA), static_cast<int>(boost::size(compressed)));
    if (compressed_size > 0) {
        socket_.async_send_to(boost::asio::buffer(compressed, compressed_size),
                              remote_endpoint_,
                              boost::bind(&udp_server::handle_send,
                                          this,
                                          boost::asio::placeholders::error,
                                          boost::asio::placeholders::bytes_transferred));
    } else {
        LOGE("send_seaarea: LZ4_compress_default() error! - %1%", compressed_size);
    }
}

void udp_server::handle_receive(const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (!error || error == boost::asio::error::message_size) {
        unsigned char type = *reinterpret_cast<unsigned char*>(recv_buffer_.data() + 0x00); // type
        if (type == 110) {
            // LPGP_LWPTTLPING
            LOGIx("PING received.");
            auto p = reinterpret_cast<LWPTTLPING*>(recv_buffer_.data());
            // ships (vessels)
            send_full_state(p->lng, p->lat, p->ex_lng, p->ex_lat, p->view_scale);
            // area titles
            send_seaarea(p->lng, p->lat);
            if (p->track_object_id || p->track_object_ship_id) {
                // tracking info
                send_track_object_coords(p->track_object_id, p->track_object_ship_id);
            }
        } else if (type == 116) {
            // LPGP_LWPTTLREQUESTWAYPOINTS
            LOGIx("REQUESTWAYPOINTS received.");
            auto p = reinterpret_cast<LWPTTLREQUESTWAYPOINTS*>(recv_buffer_.data());
            send_waypoints(p->ship_id);
            LOGIx("REQUESTWAYPOINTS replied with WAYPOINTS.");
        } else if (type == 118) {
            // LPGP_LWPTTLPINGFLUSH
            LOGI("PINGFLUSH received.");
        } else if (type == 119) {
            // LPGP_LWPTTLPINGCHUNK
            LOGIx("PINGCHUNK received.");
            auto p = reinterpret_cast<LWPTTLPINGCHUNK*>(recv_buffer_.data());
            LWTTLCHUNKKEY chunk_key;
            chunk_key.v = p->chunk_key;
            const int clamped_view_scale = boost::algorithm::clamp(1 << chunk_key.bf.view_scale_msb, 1 << 0, 1 << 6);
            const int xc0_aligned = chunk_key.bf.xcc0 << msb_index(LNGLAT_SEA_PING_EXTENT_IN_CELL_PIXELS * clamped_view_scale);
            const int yc0_aligned = chunk_key.bf.ycc0 << msb_index(LNGLAT_SEA_PING_EXTENT_IN_CELL_PIXELS * clamped_view_scale);
            const float ex_lng = LNGLAT_SEA_PING_EXTENT_IN_CELL_PIXELS;
            const float ex_lat = LNGLAT_SEA_PING_EXTENT_IN_CELL_PIXELS;
            if (p->static_object == 1) {
                // land cells
                send_land_cell_aligned(xc0_aligned, yc0_aligned, ex_lng, ex_lat, clamped_view_scale);
            } else if (p->static_object == 2) {
                // seaports
                const unsigned int ts = seaport_->query_ts(chunk_key);
                if (ts > p->ts) {
                    send_seaport_cell_aligned(xc0_aligned, yc0_aligned, ex_lng, ex_lat, clamped_view_scale);
                } else {
                    LOGI("Chunk key (%1%,%2%,%3%) Latest ts %4%",
                         static_cast<int>(chunk_key.bf.xcc0),
                         static_cast<int>(chunk_key.bf.ycc0),
                         static_cast<int>(chunk_key.bf.view_scale_msb),
                         ts);
                }
            }
        } else {
            LOGI("%1%: Unknown UDP request of type %2%",
                 __func__,
                 static_cast<int>(type));
        }
    } else {
        LOGE("%1%: error %2%, bytes_transferred %3%", __func__, error, bytes_transferred);
    }
    start_receive();
}

std::shared_ptr<route> udp_server::create_route_id(const std::vector<int>& seaport_id_list) const {
    if (seaport_id_list.size() == 0) {
        return std::shared_ptr<route>();
    }
    std::vector<seaport_object_public::point_t> point_list;
    for (auto v : seaport_id_list) {
        point_list.emplace_back(seaport_->get_seaport_point(v));
        LOGI("Seaport ID: %1% (%2%)", v, seaport_->get_seaport_name(v));
    }
    std::vector<xy32> wp_total;
    for (size_t i = 0; i < point_list.size() - 1; i++) {
        auto wp = sea_static_->calculate_waypoints(point_list[i], point_list[i + 1]);
        if (wp.size() >= 2) {
            std::copy(wp.begin(), wp.end(), std::back_inserter(wp_total));
        } else {
            LOGE("Waypoints of less than 2 detected. Route could not be found.");
            return std::shared_ptr<route>();
        }
    }
    std::shared_ptr<route> r(new route(wp_total));
    r->set_velocity(1);
    return r;
}

std::shared_ptr<route> udp_server::create_route(const std::vector<std::string>& seaport_list) const {
    if (seaport_list.size() == 0) {
        return std::shared_ptr<route>();
    }
    std::vector<seaport_object_public::point_t> point_list;
    for (auto v : seaport_list) {
        point_list.emplace_back(seaport_->get_seaport_point(v.c_str()));
        LOGI("Seaport: %1%", v);
    }
    std::vector<xy32> wp_total;
    for (size_t i = 0; i < point_list.size() - 1; i++) {
        auto wp = sea_static_->calculate_waypoints(point_list[i], point_list[i + 1]);
        if (wp.size() >= 2) {
            std::copy(wp.begin(), wp.end(), std::back_inserter(wp_total));
        } else {
            LOGE("Waypoints of less than 2 detected. Route could not be found.");
            return std::shared_ptr<route>();
        }
    }
    std::shared_ptr<route> r(new route(wp_total));
    r->set_velocity(1);
    return r;
}

std::shared_ptr<const route> udp_server::find_route_map_by_ship_id(int ship_id) const {
    auto obj = sea_->get_object_by_type(ship_id);
    if (!obj) {
        LOGE("%1%: Sea object %2% not found.",
             __func__,
             ship_id);
    } else {
        auto it = route_map_.find(obj->get_id());
        if (it != route_map_.end()) {
            return it->second;
        } else {
            LOGE("%1%: Sea object %2% has no route info.",
                 __func__,
                 ship_id);
        }
    }
    return std::shared_ptr<const route>();
}
