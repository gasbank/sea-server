#include "precompiled.hpp"
#include "udp-server.hpp"
#include "tcp-server.hpp"
#include "sea.hpp"
#include "udp-admin-server.hpp"
#include "sea_static.hpp"
#include "seaport.hpp"

using namespace ss;

int main() {
    try {
        std::cout << "sea-server v0.1" << std::endl;
        auto cwd = boost::filesystem::current_path();
        do {
            auto assets = cwd;
            assets.append("assets");
            if (boost::filesystem::is_directory(assets)) {
                boost::filesystem::current_path(cwd);
                break;
            }
            cwd.remove_leaf();
        } while (!cwd.empty());

        if (cwd.empty()) {
            abort();
        }
        
        std::cout << "Current path: " << boost::filesystem::current_path() << std::endl;
        boost::asio::io_service io_service;
        std::shared_ptr<sea> sea_instance(new sea());
        sea_instance->populate_test();
        std::shared_ptr<sea_static> sea_static_instance(new sea_static());
        std::shared_ptr<seaport> seaport_instance(new seaport());
        udp_server udp_server_instance(io_service, sea_instance, sea_static_instance, seaport_instance);
        tcp_server tcp_server_instance(io_service);
        udp_admin_server udp_admin_server(io_service,
                                          sea_instance,
                                          sea_static_instance,
                                          seaport_instance,
                                          udp_server_instance);
        io_service.run();
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
