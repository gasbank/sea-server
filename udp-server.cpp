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
    auto spawn_point = seaport_->get_seaport_point("Busan");
    auto spawn_point_x = static_cast<float>(spawn_point.get<0>());
    auto spawn_point_y = static_cast<float>(spawn_point.get<1>());
    auto id1 = sea_->spawn("Test A", 1, spawn_point_x, spawn_point_y, 1, 1);
    auto id2 = sea_->spawn("Test B", 1, spawn_point_x, spawn_point_y, 1, 1);
    auto id3 = sea_->spawn("Test C", 1, spawn_point_x, spawn_point_y, 1, 1);
    auto id4 = sea_->spawn("Test D", 1, spawn_point_x, spawn_point_y, 1, 1);
    auto id5 = sea_->spawn("Test E", 1, spawn_point_x, spawn_point_y, 1, 1);
    auto id6 = sea_->spawn("Test F", 1, spawn_point_x, spawn_point_y, 1, 1);
    auto id7 = sea_->spawn("Test G", 1, spawn_point_x, spawn_point_y, 1, 1);
    auto id8 = sea_->spawn("Test H", 1, spawn_point_x, spawn_point_y, 1, 1);

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
        "Onsan/Ulsan",
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

    std::cout << "Route setup completed." << std::endl;

    start_receive();
    timer_.async_wait(boost::bind(&udp_server::update, this));
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
    for (auto v : route_map_) {
        sea_->update_route(delta_time, v.first, v.second);
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
        std::cerr << error << std::endl;
    } else {
        //std::cout << bytes_transferred << " bytes sent." << std::endl;
    }
}

void udp_server::send_full_state(float xc, float yc, float ex) {
    std::vector<sea_object_public> sop_list;
    sea_->query_near_lng_lat_to_packet(xc, yc, ex, sop_list);

    boost::shared_ptr<LWPTTLFULLSTATE> reply(new LWPTTLFULLSTATE);
    memset(reply.get(), 0, sizeof(LWPTTLFULLSTATE));
    reply->type = 109; // LPGP_LWPTTLFULLSTATE
    size_t reply_obj_index = 0;
    for (sea_object_public const& v : sop_list) {
        reply->obj[reply_obj_index].x0 = v.x;
        reply->obj[reply_obj_index].y0 = v.y;
        reply->obj[reply_obj_index].x1 = v.x + v.w;
        reply->obj[reply_obj_index].y1 = v.y + v.h;
        reply->obj[reply_obj_index].vx = v.vx;
        reply->obj[reply_obj_index].vy = v.vy;
        reply->obj[reply_obj_index].id = v.id;
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
    //std::cout << boost::format("Querying (%1%,%2%) extent %3% => %4% hit(s).\n") % xc % yc % ex % reply_obj_index;
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
        std::cerr << boost::format("send_full_state: LZ4_compress_default() error! - %1%\n") % compressed_size;
    }
}

void udp_server::send_static_state(float xc, float yc, float ex) {
    auto sop_list = sea_static_->query_near_lng_lat_to_packet(xc, yc, ex);

    boost::shared_ptr<LWPTTLSTATICSTATE> reply(new LWPTTLSTATICSTATE);
    memset(reply.get(), 0, sizeof(LWPTTLSTATICSTATE));
    reply->type = 111; // LPGP_LWPTTLSTATICSTATE
    size_t reply_obj_index = 0;
    for (sea_static_object_public const& v : sop_list) {
        reply->obj[reply_obj_index].x0 = v.x0;
        reply->obj[reply_obj_index].y0 = v.y0;
        reply->obj[reply_obj_index].x1 = v.x1;
        reply->obj[reply_obj_index].y1 = v.y1;
        reply_obj_index++;
        if (reply_obj_index >= boost::size(reply->obj)) {
            break;
        }
    }
    reply->count = static_cast<int>(reply_obj_index);
    //std::cout << boost::format("Querying (%1%,%2%) extent %3% => %4% hit(s).\n") % xc % yc % ex % reply_obj_index;
    char compressed[1500];
    int compressed_size = LZ4_compress_default((char*)reply.get(), compressed, sizeof(LWPTTLSTATICSTATE), static_cast<int>(boost::size(compressed)));
    if (compressed_size > 0) {
        socket_.async_send_to(boost::asio::buffer(compressed, compressed_size),
                              remote_endpoint_,
                              boost::bind(&udp_server::handle_send,
                                          this,
                                          boost::asio::placeholders::error,
                                          boost::asio::placeholders::bytes_transferred));
    } else {
        std::cerr << boost::format("send_static_state: LZ4_compress_default() error! - %1%\n") % compressed_size;
    }
}

void udp_server::send_track_object_coords(int track_object_id) {
    auto obj = sea_->get_object(track_object_id);
    if (obj) {
        boost::shared_ptr<LWPTTLTRACKCOORDS> reply(new LWPTTLTRACKCOORDS);
        memset(reply.get(), 0, sizeof(LWPTTLTRACKCOORDS));
        reply->type = 113; // LPGP_LWPTTLTRACKCOORDS
        reply->id = track_object_id;
        obj->get_xy(reply->x, reply->y);
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
            std::cerr << boost::format("send_track_object_coords: LZ4_compress_default() error! - %1%\n") % compressed_size;
        }
    }
}

void udp_server::send_seaport(float xc, float yc, float ex) {
    auto sop_list = seaport_->query_near_lng_lat_to_packet(xc, yc, static_cast<int>(ex / 2));

    boost::shared_ptr<LWPTTLSEAPORTSTATE> reply(new LWPTTLSEAPORTSTATE);
    memset(reply.get(), 0, sizeof(LWPTTLSEAPORTSTATE));
    reply->type = 112; // LPGP_LWPTTLSEAPORTSTATE
    size_t reply_obj_index = 0;
    for (seaport_object_public const& v : sop_list) {
        reply->obj[reply_obj_index].x0 = v.x0;
        reply->obj[reply_obj_index].y0 = v.y0;
        strcpy(reply->obj[reply_obj_index].name, seaport_->get_seaport_name(v.id));
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
        std::cerr << boost::format("send_seaport: LZ4_compress_default() error! - %1%\n") % compressed_size;
    }
}

void udp_server::send_seaarea(float xc, float yc) {
    std::string area_name;
    region_->query_tree(xc, yc, area_name);

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
        std::cerr << boost::format("send_seaarea: LZ4_compress_default() error! - %1%\n") % compressed_size;
    }
}

void udp_server::handle_receive(const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (!error || error == boost::asio::error::message_size) {
        unsigned char type = *reinterpret_cast<unsigned char*>(recv_buffer_.data() + 0x00); // type
        if (type == 110) {
            // LPGP_LWPTTLPING
            //std::cout << "PING received." << std::endl;
            unsigned char padding0 = *reinterpret_cast<unsigned char*>(recv_buffer_.data() + 0x01);
            unsigned char padding1 = *reinterpret_cast<unsigned char*>(recv_buffer_.data() + 0x02);
            unsigned char padding2 = *reinterpret_cast<unsigned char*>(recv_buffer_.data() + 0x03);
            float xc = *reinterpret_cast<float*>(recv_buffer_.data() + 0x04); // x center
            float yc = *reinterpret_cast<float*>(recv_buffer_.data() + 0x08); // y center
            float ex = *reinterpret_cast<float*>(recv_buffer_.data() + 0x0c); // extent
            int ping_seq = *reinterpret_cast<int*>(recv_buffer_.data() + 0x10);
            int track_object_id = *reinterpret_cast<int*>(recv_buffer_.data() + 0x14);

            send_full_state(xc, yc, ex);
            if (ping_seq % 64 == 0) {
                send_static_state(xc, yc, ex);
                send_seaport(xc, yc, ex);
                send_seaarea(xc, yc);
            }
            if (track_object_id) {
                send_track_object_coords(track_object_id);
            }
            //std::cout << "STATE replied." << std::endl;
        }
        start_receive();
    }
}

std::shared_ptr<route> udp_server::create_route_id(const std::vector<int>& seaport_id_list) const {
    if (seaport_id_list.size() == 0) {
        return std::shared_ptr<route>();
    }
    std::vector<seaport_object_public::point_t> point_list;
    for (auto v : seaport_id_list) {
        point_list.emplace_back(seaport_->get_seaport_point(v));
        std::cout << boost::format("Seaport ID: %1%\n") % v;
    }
    std::vector<xy32> wp_total;
    for (size_t i = 0; i < point_list.size() - 1; i++) {
        auto wp = sea_static_->calculate_waypoints(point_list[i], point_list[i + 1]);
        if (wp.size() >= 2) {
            std::copy(wp.begin(), wp.end(), std::back_inserter(wp_total));
        } else {
            std::cerr << "Waypoints of less than 2 detected. Route could not be found." << std::endl;
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
        std::cout << boost::format("Seaport: %1%\n") % v.c_str();
    }
    std::vector<xy32> wp_total;
    for (size_t i = 0; i < point_list.size() - 1; i++) {
        auto wp = sea_static_->calculate_waypoints(point_list[i], point_list[i + 1]);
        if (wp.size() >= 2) {
            std::copy(wp.begin(), wp.end(), std::back_inserter(wp_total));
        } else {
            std::cerr << "Waypoints of less than 2 detected. Route could not be found." << std::endl;
            return std::shared_ptr<route>();
        }
    }
    std::shared_ptr<route> r(new route(wp_total));
    r->set_velocity(1);
    return r;
}
