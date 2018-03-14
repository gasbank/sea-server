#pragma once

namespace ss {
    namespace bg = boost::geometry;
    namespace bgi = boost::geometry::index;
    using boost::asio::ip::udp;
    class sea;

    class udp_server {

    public:
        udp_server(boost::asio::io_service& io_service, std::shared_ptr<sea> sea);

    private:
        void update();
        void start_receive();

        // How to test handle_receive():
        // $ perl -e "print pack('ff',10.123,20.456)" > /dev/udp/127.0.0.1/3100

        void handle_receive(const boost::system::error_code& error,
                            std::size_t bytes_transferred);

        void handle_send(const boost::system::error_code& error,
                         std::size_t bytes_transferred);

        udp::socket socket_;
        udp::endpoint remote_endpoint_;
        boost::array<char, 1024> recv_buffer_;
        boost::asio::deadline_timer timer_;
        std::shared_ptr<sea> sea_;
    };
}
