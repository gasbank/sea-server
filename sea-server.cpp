#include "precompiled.hpp"
#include "udp-server.hpp"
#include "tcp-server.hpp"
#include "sea.hpp"

using namespace ss;

int main() {
    try {
        std::cout << "sea-server v0.1" << std::endl;
        boost::asio::io_service io_service;
        std::shared_ptr<sea> sea_instance(new sea());
        sea_instance->populate_test();
        udp_server udp_server(io_service, sea_instance);
        tcp_server tcp_server(io_service);
        io_service.run();
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
