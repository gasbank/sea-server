#include "precompiled.hpp"
#include "udp-server.hpp"
#include "tcp-server.hpp"
#include "sea.hpp"

using namespace ss;

int main() {
    try {
        std::cout << "sea-server v0.1" << std::endl;
        boost::asio::io_context io_context;
        std::shared_ptr<sea> sea(new sea());
        sea->populate_test();
        udp_server udp_server(io_context, sea);
        tcp_server tcp_server(io_context);
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
