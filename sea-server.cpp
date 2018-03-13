#include <ctime>
#include <iostream>
#include <string>
#include <vector>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/random.hpp>

namespace bg = boost::geometry;
namespace bgi = boost::geometry::index;
using boost::asio::ip::udp;
typedef bg::model::point<float, 2, bg::cs::cartesian> point;
typedef bg::model::box<point> box;
typedef std::pair<box, unsigned> value;

std::string make_daytime_string() {
    using namespace std; // For time_t, time and ctime;
    time_t now = time(0);
    return ctime(&now);
}

struct SEAOBJECT {
    float x0, y0;
    float x1, y1;
    int id;
};
struct SEAPINGREPLY {
    int count;
    SEAOBJECT obj[64];
};

class udp_server {
public:
    udp_server(boost::asio::io_context& io_context)
    : socket_(io_context, udp::endpoint(udp::v4(), 3100)),
    timer(io_context, boost::posix_time::seconds(1)) {
        // create some values
        boost::random::mt19937 rng;
        boost::random::uniform_real_distribution<float> random_point(-1000,1000);
        for (unsigned i = 0; i < 100000; ++i) {
            // create a box
            float x = random_point(rng);
            float y = random_point(rng);
            box b(point(x, y), point(x + 0.5f, y + 0.5f));
            // insert new value
            rtree.insert(std::make_pair(b, i));
        }
        start_receive();
        timer.async_wait(boost::bind(&udp_server::update, this));
    }
    
private:
    void update() {
        //std::cout << "update..." << std::endl;
        timer.expires_at(timer.expires_at() + boost::posix_time::seconds(1));
        timer.async_wait(boost::bind(&udp_server::update, this));
    }
    void start_receive() {
        socket_.async_receive_from(
                                   boost::asio::buffer(recv_buffer_), remote_endpoint_,
                                   boost::bind(&udp_server::handle_receive, this,
                                               boost::asio::placeholders::error,
                                               boost::asio::placeholders::bytes_transferred));
    }
    
    // How to test handle_receive():
    // $ perl -e "print pack('ff',10.123,20.456)" > /dev/udp/127.0.0.1/3100
    
    void handle_receive(const boost::system::error_code& error,
                        std::size_t bytes_transferred) {
        if (!error || error == boost::asio::error::message_size) {
            float xc = *reinterpret_cast<float*>(recv_buffer_.data() + 0); // x center
            float yc = *reinterpret_cast<float*>(recv_buffer_.data() + 4); // y center
            float ex = *reinterpret_cast<float*>(recv_buffer_.data() + 8); // extent
            // find values intersecting some area defined by a box
            box query_box(point(xc-ex/2, yc-ex/2), point(xc+ex/2, yc+ex/2));
            std::vector<value> result_s;
            rtree.query(bgi::intersects(query_box), std::back_inserter(result_s));
            
            boost::shared_ptr<SEAPINGREPLY> reply(new SEAPINGREPLY);
            reply->count = result_s.size();
            int reply_obj_index = 0;
            BOOST_FOREACH(value const& v, result_s) {
                reply->obj[reply_obj_index].x0 = v.first.min_corner().get<0>();
                reply->obj[reply_obj_index].y0 = v.first.min_corner().get<1>();
                reply->obj[reply_obj_index].x1 = v.first.max_corner().get<0>();
                reply->obj[reply_obj_index].y1 = v.first.max_corner().get<1>();
                reply->obj[reply_obj_index].id = result_s[reply_obj_index].second;
                reply_obj_index++;
                if (reply_obj_index >= sizeof(reply->obj) / sizeof(SEAOBJECT)) {
                    break;
                }
            }
            std::cout << boost::format("Querying (%1%,%2%) extent %3% => %4% hit(s).\n") % xc % yc % ex % reply_obj_index;
            boost::asio::const_buffer send_buf(reply.get(), sizeof(SEAPINGREPLY));
            
            boost::shared_ptr<std::string> message(new std::string(make_daytime_string()));
            
            socket_.async_send_to(send_buf, remote_endpoint_,
                                  boost::bind(&udp_server::handle_send, this, send_buf,
                                              boost::asio::placeholders::error,
                                              boost::asio::placeholders::bytes_transferred));
            
            start_receive();
        }
    }
    
    void handle_send(boost::asio::const_buffer /*message*/,
                     const boost::system::error_code& error,
                     std::size_t bytes_transferred) {
        if (error) {
            std::cerr << error << std::endl;
        } else {
            std::cout << bytes_transferred << " bytes transferred." << std::endl;
        }
    }
    
    udp::socket socket_;
    udp::endpoint remote_endpoint_;
    boost::array<char, 1024> recv_buffer_;
    bgi::rtree< value, bgi::quadratic<16> > rtree;
    boost::asio::deadline_timer timer;
};

int main() {
    try {
        boost::asio::io_context io_context;
        udp_server server(io_context);
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    
    return 0;
}

