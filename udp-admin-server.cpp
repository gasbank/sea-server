#include "precompiled.hpp"
#include "udp-admin-server.hpp"
#include "sea.hpp"
#include "lz4.h"
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

udp_admin_server::udp_admin_server(boost::asio::io_service & io_service, std::shared_ptr<sea> sea)
    : socket_(io_service, udp::endpoint(udp::v4(), 4000)),
    sea_(sea) {
    start_receive();
}

void udp_admin_server::start_receive() {
    socket_.async_receive_from(boost::asio::buffer(recv_buffer_),
                               remote_endpoint_,
                               boost::bind(&udp_admin_server::handle_receive,
                                           this,
                                           boost::asio::placeholders::error,
                                           boost::asio::placeholders::bytes_transferred));
}

void udp_admin_server::handle_send(const boost::system::error_code & error, std::size_t bytes_transferred) {
    if (error) {
        std::cerr << error << std::endl;
    } else {
        //std::cout << bytes_transferred << " bytes transferred." << std::endl;
    }
}

struct command {
    char type;
    char padding0;
    char padding1;
    char padding2;
};

struct spawn_command {
    command _;
    char guid[64];
    float x;
    float y;
};

struct travel_to_command {
    command _;
    char guid[64];
    float x;
    float y;
};

struct teleport_to_command {
    command _;
    char guid[64];
    float x;
    float y;
};

struct spawn_ship_command {
    command _;
    int id;
    char name[64];
    float x;
    float y;
};

void udp_admin_server::handle_receive(const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (!error || error == boost::asio::error::message_size) {
        command* cp = reinterpret_cast<command*>(recv_buffer_.data());
        switch (cp->type) {
        case 1: // Spawn
        {
            assert(bytes_transferred == sizeof(spawn_command));
            std::cout << boost::format("Spawn type: %1%\n") % static_cast<int>(cp->type);
            spawn_command* spawn = reinterpret_cast<spawn_command*>(recv_buffer_.data());;
            int type = 1;
            sea_->spawn(spawn->guid, type, spawn->x, spawn->y, 1.0f, 1.0f);
            break;
        }
        case 2: // Travel To
        {
            assert(bytes_transferred == sizeof(travel_to_command));
            std::cout << boost::format("Travel To type: %1%\n") % static_cast<int>(cp->type);
            travel_to_command* spawn = reinterpret_cast<travel_to_command*>(recv_buffer_.data());;
            sea_->travel_to(spawn->guid, spawn->x, spawn->y);
            break;
        }
        case 3: // Telport To
        {
            assert(bytes_transferred == sizeof(teleport_to_command));
            std::cout << boost::format("Teleport To type: %1%\n") % static_cast<int>(cp->type);
            teleport_to_command* spawn = reinterpret_cast<teleport_to_command*>(recv_buffer_.data());;
            sea_->teleport_to(spawn->guid, spawn->x, spawn->y);
            break;
        }
        case 4: // Spawn Ship
        {
            assert(bytes_transferred == sizeof(spawn_ship_command));
            std::cout << boost::format("Spawn Ship type: %1%\n") % static_cast<int>(cp->type);
            spawn_ship_command* spawn = reinterpret_cast<spawn_ship_command*>(recv_buffer_.data());;
            sea_->spawn(spawn->id, spawn->x, spawn->y, 1, 1);
            break;
        }
        default:
        {
            std::cerr << boost::format("Unknown command packet type: %1%\n") % static_cast<int>(cp->type);
            break;
        }
        }
        start_receive();
    }
}
