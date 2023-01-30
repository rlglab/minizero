#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <cstdio>
#include <ctime>
#include <string>

namespace minizero::utils {

class TimeSystem {
public:
    static boost::posix_time::ptime getLocalTime() { return boost::posix_time::microsec_clock::local_time(); }

    static std::string getTimeString(std::string format = "Y/m/d H:i:s", boost::posix_time::ptime local_time = getLocalTime())
    {
        std::string time_string;
        bool is_escape = false;

        for (size_t i = 0; i < format.length(); ++i) {
            if (is_escape) {
                time_string += format.at(i);
                is_escape = false;
            } else {
                switch (format.at(i)) {
                    case 'Y': time_string += translateIntToString(local_time.date().year()); break;
                    case 'y': time_string += translateIntToString(local_time.date().year() % 100, 2); break;
                    case 'm': time_string += translateIntToString(local_time.date().month(), 2); break;
                    case 'd': time_string += translateIntToString(local_time.date().day(), 2); break;
                    case 'H': time_string += translateIntToString(local_time.time_of_day().hours(), 2); break;
                    case 'i': time_string += translateIntToString(local_time.time_of_day().minutes(), 2); break;
                    case 's': time_string += translateIntToString(local_time.time_of_day().seconds(), 2); break;
                    case 'f': time_string += translateIntToString(local_time.time_of_day().total_milliseconds() % 1000, 3); break;
                    case 'u': time_string += translateIntToString(local_time.time_of_day().total_microseconds() % 1000000, 6); break;
                    case '\\': is_escape = true; break;
                    default: time_string += format.at(i); break;
                }
            }
        }
        return time_string;
    }

private:
    static std::string translateIntToString(int value, int width = 0)
    {
        char buf[16];
        static char zero_fill_format[] = "%0*d", non_zero_fill_format[] = "%*d";

        if (width > 15) width = 15;
        if (width < 0) width = 0;

        char* format = (width ? zero_fill_format : non_zero_fill_format);
        snprintf(buf, sizeof(buf), format, width, value);
        return buf;
    }
};

} // namespace minizero::utils
