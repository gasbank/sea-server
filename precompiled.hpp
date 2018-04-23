#pragma once
#include <ctime>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
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
#include <boost/range.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/clamp.hpp>
#include <boost/crc.hpp>
#include <boost/math/special_functions/round.hpp>

#define WORLD_MAP_PIXEL_RESOLUTION_WIDTH (172824)
#define WORLD_MAP_PIXEL_RESOLUTION_HEIGHT (86412)
#define WORLD_CIRCUMFERENCE_IN_KM (40075.0f)

namespace ss {
    static std::string awesome_printf_helper(boost::format& f) {
        return boost::str(f);
    }

    template<class T, class... Args>
    std::string awesome_printf_helper(boost::format& f, T&& t, Args&&... args) {
        try {
            return awesome_printf_helper(f % std::forward<T>(t), std::forward<Args>(args)...);
        } catch (boost::io::bad_format_string& e) {
            std::cerr << e.what() << std::endl;
            std::cerr << "format: " << f << ", T: " << t << std::endl;
            return "";
        }
    }

    template<typename... Arguments>
    void LOG(std::ostream& stream, const std::string& fmt, Arguments&&... args) {
        try {
            boost::format f(fmt);
            stream << awesome_printf_helper(f, std::forward<Arguments>(args)...) << std::endl;
        } catch (boost::io::bad_format_string& e) {
            std::cerr << e.what() << std::endl;
            std::cerr << "fmt: " << fmt << std::endl;
        }
    }

    template<typename... Arguments>
    void LOGI(const std::string& fmt, Arguments&&... args) {
        LOG(std::cout, fmt, std::forward<Arguments>(args)...);
    }

    template<typename... Arguments>
    void LOGE(const std::string& fmt, Arguments&&... args) {
        LOG(std::cerr, fmt, std::forward<Arguments>(args)...);
    }

    template<typename... Arguments>
    void LOGIx(const std::string& fmt, Arguments&&... args) {
    }

    template<typename... Arguments>
    void LOGEx(const std::string& fmt, Arguments&&... args) {
    }
}
