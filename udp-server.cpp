#include "precompiled.hpp"
#include "udp-server.hpp"
#include "sea.hpp"
#include "sea_static.hpp"
#include "seaport.hpp"
#include "lz4.h"
#include "route.hpp"
#include "xy.hpp"

using namespace ss;

typedef struct _LWPTTLFULLSTATEOBJECT {
    float x0, y0;
    float x1, y1;
    float vx, vy;
    int id;
} LWPTTLFULLSTATEOBJECT;

typedef struct _LWPTTLFULLSTATE {
    unsigned char type;
    unsigned char padding0;
    unsigned char padding1;
    unsigned char padding2;
    int count;
    LWPTTLFULLSTATEOBJECT obj[32];
} LWPTTLFULLSTATE;

typedef struct _LWPTTLSTATICOBJECT {
    short x0;
    short y0;
    short x1;
    short y1;
} LWPTTLSTATICOBJECT;

typedef struct _LWPTTLSTATICSTATE {
    unsigned char type;
    unsigned char padding0;
    unsigned char padding1;
    unsigned char padding2;
    int count;
    LWPTTLSTATICOBJECT obj[200];
} LWPTTLSTATICSTATE;

typedef struct _LWPTTLSEAPORTOBJECT {
    short x0;
    short y0;
    char name[64];
} LWPTTLSEAPORTOBJECT;

typedef struct _LWPTTLSEAPORTSTATE {
    unsigned char type;
    unsigned char padding0;
    unsigned char padding1;
    unsigned char padding2;
    int count;
    LWPTTLSEAPORTOBJECT obj[200];
} LWPTTLSEAPORTSTATE;

static std::string make_daytime_string() {
    using namespace std; // For time_t, time and ctime;
    time_t now = time(0);
    return ctime(&now);
}

const auto update_interval = boost::posix_time::seconds(1);
//const auto update_interval = boost::posix_time::milliseconds(250);

udp_server::udp_server(boost::asio::io_service & io_service,
                       std::shared_ptr<sea> sea,
                       std::shared_ptr<sea_static> sea_static,
                       std::shared_ptr<seaport> seaport)
    : socket_(io_service, udp::endpoint(udp::v4(), 3100))
    , timer_(io_service, update_interval)
    , sea_(sea)
    , sea_static_(sea_static)
    , seaport_(seaport)
{
    //auto wp = sea_static_->calculate_waypoints(xy{ 14083,2476 }, xy{ 14079,2480 });
    auto wp = sea_static_->calculate_waypoints(xy{ 14083,2476 }, xy{ 13952,2517 });
    route_.reset(new route(wp));
    route_->set_velocity(1);

    start_receive();
    timer_.async_wait(boost::bind(&udp_server::update, this));
}

void udp_server::update() {
    //std::cout << "update..." << std::endl;
    sea_->update(update_interval.total_milliseconds() / 1000.0f);
    timer_.expires_at(timer_.expires_at() + update_interval);
    timer_.async_wait(boost::bind(&udp_server::update, this));
    route_->update(1);
    auto pos = route_->get_pos();
    sea_->teleport_to("Test A", pos.first, pos.second);
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
        //std::cout << bytes_transferred << " bytes transferred." << std::endl;
    }
}

void udp_server::send_full_state(float xc, float yc, float ex) {
    std::vector<sea_object_public> sop_list;
    sea_->query_near_lng_lat_to_packet(xc, yc, static_cast<short>(ex / 2), sop_list);

    boost::shared_ptr<LWPTTLFULLSTATE> reply(new LWPTTLFULLSTATE);
    memset(reply.get(), 0, sizeof(LWPTTLFULLSTATE));
    reply->type = 109; // LPGP_LWPTTLFULLSTATE
    reply->count = sop_list.size();
    size_t reply_obj_index = 0;
    BOOST_FOREACH(sea_object_public const& v, sop_list) {
        reply->obj[reply_obj_index].x0 = v.x;
        reply->obj[reply_obj_index].y0 = v.y;
        reply->obj[reply_obj_index].x1 = v.x + v.w;
        reply->obj[reply_obj_index].y1 = v.y + v.h;
        reply->obj[reply_obj_index].vx = v.vx;
        reply->obj[reply_obj_index].vy = v.vy;
        reply->obj[reply_obj_index].id = v.id;
        reply_obj_index++;
        if (reply_obj_index >= boost::size(reply->obj)) {
            break;
        }
    }
    //std::cout << boost::format("Querying (%1%,%2%) extent %3% => %4% hit(s).\n") % xc % yc % ex % reply_obj_index;
    char compressed[1500];
    int compressed_size = LZ4_compress_default((char*)reply.get(), compressed, sizeof(LWPTTLFULLSTATE), boost::size(compressed));
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
    auto sop_list = sea_static_->query_near_lng_lat_to_packet(xc, yc, static_cast<short>(ex / 2));
    
    boost::shared_ptr<LWPTTLSTATICSTATE> reply(new LWPTTLSTATICSTATE);
    memset(reply.get(), 0, sizeof(LWPTTLSTATICSTATE));
    reply->type = 111; // LPGP_LWPTTLSTATICSTATE
    reply->count = sop_list.size();
    size_t reply_obj_index = 0;
    BOOST_FOREACH(sea_static_object_public const& v, sop_list) {
        reply->obj[reply_obj_index].x0 = v.x0;
        reply->obj[reply_obj_index].y0 = v.y0;
        reply->obj[reply_obj_index].x1 = v.x1;
        reply->obj[reply_obj_index].y1 = v.y1;
        reply_obj_index++;
        if (reply_obj_index >= boost::size(reply->obj)) {
            break;
        }
    }
    //std::cout << boost::format("Querying (%1%,%2%) extent %3% => %4% hit(s).\n") % xc % yc % ex % reply_obj_index;
    char compressed[1500];
    int compressed_size = LZ4_compress_default((char*)reply.get(), compressed, sizeof(LWPTTLSTATICSTATE), boost::size(compressed));
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

void udp_server::send_seaport(float xc, float yc, float ex) {
    auto sop_list = seaport_->query_near_lng_lat_to_packet(xc, yc, static_cast<short>(ex / 2));

    boost::shared_ptr<LWPTTLSEAPORTSTATE> reply(new LWPTTLSEAPORTSTATE);
    memset(reply.get(), 0, sizeof(LWPTTLSEAPORTSTATE));
    reply->type = 112; // LPGP_LWPTTLSEAPORTSTATE
    reply->count = sop_list.size();
    size_t reply_obj_index = 0;
    BOOST_FOREACH(seaport_object_public const& v, sop_list) {
        reply->obj[reply_obj_index].x0 = v.x0;
        reply->obj[reply_obj_index].y0 = v.y0;
        strcpy(reply->obj[reply_obj_index].name, seaport_->get_seaport_name(v.id));
        reply_obj_index++;
        if (reply_obj_index >= boost::size(reply->obj)) {
            break;
        }
    }
    char compressed[1500];
    int compressed_size = LZ4_compress_default((char*)reply.get(), compressed, sizeof(LWPTTLSEAPORTSTATE), boost::size(compressed));
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

void udp_server::handle_receive(const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (!error || error == boost::asio::error::message_size) {
        float cd = *reinterpret_cast<float*>(recv_buffer_.data() + 0x00); // command type (?)
        float xc = *reinterpret_cast<float*>(recv_buffer_.data() + 0x04); // x center
        float yc = *reinterpret_cast<float*>(recv_buffer_.data() + 0x08); // y center
        float ex = *reinterpret_cast<float*>(recv_buffer_.data() + 0x0c); // extent

        send_full_state(xc, yc, ex);
        send_static_state(xc, yc, ex);
        send_seaport(xc, yc, ex);

        start_receive();
    }
}
