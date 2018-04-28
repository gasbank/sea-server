#include "precompiled.hpp"
#include "udp-admin-server.hpp"
#include "sea.hpp"
#include "lz4.h"
#include "sea_static.hpp"
#include "xy.hpp"
#include "seaport.hpp"
#include "udp-server.hpp"
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

udp_admin_server::udp_admin_server(boost::asio::io_service & io_service,
                                   std::shared_ptr<sea> sea,
                                   std::shared_ptr<sea_static> sea_static,
                                   std::shared_ptr<seaport> seaport,
                                   udp_server& udp_server)
    : socket_(io_service, udp::endpoint(udp::v4(), 4000))
    , sea_(sea)
    , sea_static_(sea_static)
    , seaport_(seaport)
    , udp_server_(udp_server)
    , resolver_(io_service)
    , web_server_query_(udp::v4(), "localhost", "3003")
    , web_server_endpoint_(*resolver_.resolve(web_server_query_))
    , web_server_socket_(io_service) {
    start_receive();
    web_server_socket_.open(udp::v4());
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
        LOGE("ERROR: %s", error);
    } else {
        LOGIx("%1% bytes_transferred", bytes_transferred);
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
    int port1_id;
    int port2_id;
};

struct spawn_ship_command_reply {
    command _;
    int ship_id;
    int port1_id;
    int port2_id;
};

struct delete_ship_command {
    command _;
    int ship_id;
};

struct spawn_port_command {
    command _;
    int id;
    char name[64];
    int xc;
    int yc;
};

void udp_admin_server::handle_receive(const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (!error || error == boost::asio::error::message_size) {
        command* cp = reinterpret_cast<command*>(recv_buffer_.data());
        switch (cp->type) {
        case 1: // Spawn
        {
            assert(bytes_transferred == sizeof(spawn_command));
            LOGI("Spawn type: %1%", static_cast<int>(cp->type));
            spawn_command* spawn = reinterpret_cast<spawn_command*>(recv_buffer_.data());;
            int type = 1;
            sea_->spawn(spawn->guid, type, spawn->x, spawn->y, 1.0f, 1.0f);
            break;
        }
        case 2: // Travel To
        {
            assert(bytes_transferred == sizeof(travel_to_command));
            LOGI("Travel To type: %1%", static_cast<int>(cp->type));
            travel_to_command* spawn = reinterpret_cast<travel_to_command*>(recv_buffer_.data());;
            sea_->travel_to(spawn->guid, spawn->x, spawn->y);
            break;
        }
        case 3: // Telport To
        {
            assert(bytes_transferred == sizeof(teleport_to_command));
            LOGI("Teleport To type: %1%", static_cast<int>(cp->type));
            teleport_to_command* spawn = reinterpret_cast<teleport_to_command*>(recv_buffer_.data());;
            sea_->teleport_to(spawn->guid, spawn->x, spawn->y);
            break;
        }
        case 4: // Spawn Ship
        {
            assert(bytes_transferred == sizeof(spawn_ship_command));
            LOGI("Spawn Ship type: %1%", static_cast<int>(cp->type));
            const spawn_ship_command* spawn = reinterpret_cast<spawn_ship_command*>(recv_buffer_.data());;
            xy32 spawn_pos = { static_cast<int>(spawn->x), static_cast<int>(spawn->y) };
            if (sea_static_->is_water(spawn_pos)) {
                int id = sea_->spawn(spawn->id, spawn->x, spawn->y, 1, 1);
                int id1 = spawn->port1_id;
                int id2 = spawn->port2_id;
                bool new_spawn = false;
                if (spawn->port1_id == -1 || spawn->port2_id == -1) {
                    std::string port1, port2;
                    if (seaport_->get_nearest_two(spawn_pos, id1, port1, id2, port2) == 2) {
                        LOGI("Nearest two ports: %1%(id=%2%), %3%(id=%4%)",
                             port1,
                             id1,
                             port2,
                             id2);
                        new_spawn = true;
                    }
                }
                if (id1 >= 0 && id2 >= 0 && udp_server_.set_route(id, id1, id2)) {
                    spawn_ship_command_reply reply;
                    memset(&reply, 0, sizeof(spawn_ship_command_reply));
                    reply._.type = 1;
                    reply.ship_id = spawn->id;
                    reply.port1_id = id1;
                    reply.port2_id = id2;
                    if (new_spawn) {
                        socket_.async_send_to(boost::asio::buffer(&reply, sizeof(spawn_ship_command_reply)), remote_endpoint_,
                                              boost::bind(&udp_admin_server::handle_send, this,
                                                          boost::asio::placeholders::error,
                                                          boost::asio::placeholders::bytes_transferred));
                    }
                } else {
                    LOGE("Cannot get nearest two ports! port1_id=%1%, port2_id=%2%",
                         id1,
                         id2);
                }
            } else {
                LOGE("Spawn position should be water. (x=%1%, y=%2%)",
                     spawn_pos.x,
                     spawn_pos.y);
            }
            break;
        }
        case 5: // Delete Ship
        {
            assert(bytes_transferred == sizeof(delete_ship_command));
            LOGI("Delete Ship type: %1%", static_cast<int>(cp->type));
            const delete_ship_command* spawn = reinterpret_cast<delete_ship_command*>(recv_buffer_.data());;
            sea_->despawn(spawn->ship_id);
            break;
        }
        case 6: // Spawn Port
        {
            assert(bytes_transferred == sizeof(spawn_port_command));
            LOGI("Spawn Port type: %1%", static_cast<int>(cp->type));
            const spawn_port_command* spawn = reinterpret_cast<spawn_port_command*>(recv_buffer_.data());;
            xy32 spawn_pos = { spawn->xc, spawn->yc };
            if (sea_static_->is_water(spawn_pos)) {
                int id = seaport_->spawn(spawn->name, spawn->xc, spawn->yc);
                bool new_spawn = false;
                if (id >= 0) {
                    LOGI("New port spawned. (x=%1%, y=%2%)",
                         spawn_pos.x,
                         spawn_pos.y);
                } else {
                    LOGE("New port cannot be spawned at (x=%1%, y=%2%)",
                         spawn_pos.x,
                         spawn_pos.y);
                }
            } else {
                LOGE("Spawn position should be water. (x=%1%, y=%2%)",
                     spawn_pos.x,
                     spawn_pos.y);
            }
            break;
        }
        default:
        {
            LOGE("Unknown command packet type: %1%", static_cast<int>(cp->type));
            break;
        }
        }
        start_receive();
    }
}

void udp_admin_server::send_recover_all_ships() {
    boost::array<char, 1> send_buf = { 2 }; // RecoverAllShips
    web_server_socket_.send_to(boost::asio::buffer(send_buf), web_server_endpoint_);
}

void udp_admin_server::send_arrival(int ship_id) {
    struct {
        char type; char padding0; char padding1; char padding2;
        int ship_id;
    } p;
    p.type = 3;
    p.ship_id = ship_id;
    web_server_socket_.send_to(boost::asio::buffer(&p, sizeof(p)), web_server_endpoint_);
}
