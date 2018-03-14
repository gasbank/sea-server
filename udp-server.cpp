#include "precompiled.hpp"
#include "udp-server.hpp"
#include "sea.hpp"

using namespace ss;

typedef struct _LWPTTLFULLSTATEOBJECT {
    float x0, y0;
    float x1, y1;
    int id;
} LWPTTLFULLSTATEOBJECT;

typedef struct _LWPTTLFULLSTATE {
    unsigned char type;
    unsigned char padding0;
    unsigned char padding1;
    unsigned char padding2;
    int count;
    LWPTTLFULLSTATEOBJECT obj[64];
} LWPTTLFULLSTATE;

static std::string make_daytime_string() {
    using namespace std; // For time_t, time and ctime;
    time_t now = time(0);
    return ctime(&now);
}

udp_server::udp_server(boost::asio::io_service & io_service, std::shared_ptr<sea> sea)
    : socket_(io_service, udp::endpoint(udp::v4(), 3100)),
    timer_(io_service, boost::posix_time::seconds(1)),
    sea_(sea) {
    start_receive();
    timer_.async_wait(boost::bind(&udp_server::update, this));
}

void udp_server::update() {
    //std::cout << "update..." << std::endl;
    timer_.expires_at(timer_.expires_at() + boost::posix_time::seconds(1));
    timer_.async_wait(boost::bind(&udp_server::update, this));
}

void udp_server::start_receive() {
    socket_.async_receive_from(boost::asio::buffer(recv_buffer_),
                               remote_endpoint_,
                               boost::bind(&udp_server::handle_receive,
                                           this,
                                           boost::asio::placeholders::error,
                                           boost::asio::placeholders::bytes_transferred));
}

void udp_server::handle_send(boost::asio::const_buffer, const boost::system::error_code & error, std::size_t bytes_transferred) {
    if (error) {
        std::cerr << error << std::endl;
    } else {
        //std::cout << bytes_transferred << " bytes transferred." << std::endl;
    }
}

void udp_server::handle_receive(const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (!error || error == boost::asio::error::message_size) {
        float cd = *reinterpret_cast<float*>(recv_buffer_.data() + 0x00); // x center
        float xc = *reinterpret_cast<float*>(recv_buffer_.data() + 0x04); // x center
        float yc = *reinterpret_cast<float*>(recv_buffer_.data() + 0x08); // y center
        float ex = *reinterpret_cast<float*>(recv_buffer_.data() + 0x0c); // extent

        std::vector<sea_object_public> sop_list;
        sea_->query_near_to_packet(xc, yc, ex, sop_list);

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
            reply->obj[reply_obj_index].id = v.id;
            reply_obj_index++;
            if (reply_obj_index >= boost::size(reply->obj)) {
                break;
            }
        }
        //std::cout << boost::format("Querying (%1%,%2%) extent %3% => %4% hit(s).\n") % xc % yc % ex % reply_obj_index;
        boost::asio::const_buffer send_buf(reply.get(), sizeof(LWPTTLFULLSTATE));
        
        socket_.async_send_to(send_buf,
                              remote_endpoint_,
                              boost::bind(&udp_server::handle_send,
                                          this,
                                          send_buf,
                                          boost::asio::placeholders::error,
                                          boost::asio::placeholders::bytes_transferred));

        start_receive();
    }
}
