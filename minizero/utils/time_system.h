#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <cstdio>
#include <ctime>
#include <string>

class TimeSystem {
public:
    static boost::posix_time::ptime GetLocalTime() { return boost::posix_time::microsec_clock::local_time(); }

    static std::string GetTimeString(std::string format = "Y/m/d H:i:s", boost::posix_time::ptime local_time = GetLocalTime())
    {
        std::string time_string;
        bool is_escape = false;

        for (size_t i = 0; i < format.length(); ++i) {
            if (is_escape) {
                time_string += format.at(i);
                is_escape = false;
            } else {
                switch (format.at(i)) {
                    case 'Y': time_string += TranslateIntToString(local_time.date().year()); break;
                    case 'y': time_string += TranslateIntToString(local_time.date().year() % 100, 2); break;
                    case 'm': time_string += TranslateIntToString(local_time.date().month(), 2); break;
                    case 'd': time_string += TranslateIntToString(local_time.date().day(), 2); break;
                    case 'H': time_string += TranslateIntToString(local_time.time_of_day().hours(), 2); break;
                    case 'i': time_string += TranslateIntToString(local_time.time_of_day().minutes(), 2); break;
                    case 's': time_string += TranslateIntToString(local_time.time_of_day().seconds(), 2); break;
                    case 'f': time_string += TranslateIntToString(local_time.time_of_day().total_milliseconds() % 1000, 3); break;
                    case 'u': time_string += TranslateIntToString(local_time.time_of_day().total_microseconds() % 1000000, 6); break;
                    case '\\': is_escape = true; break;
                    default: time_string += format.at(i); break;
                }
            }
        }
        return time_string;
    }

private:
    static std::string TranslateIntToString(int value, int width = 0)
    {
        char buf[16];
        static char zero_fill_format[] = "%0*d", non_zero_fill_format[] = "%*d";

        if (width > 15) width = 15;
        if (width < 0) width = 0;

        char* format = (width ? zero_fill_format : non_zero_fill_format);
        sprintf(buf, format, width, value);
        return buf;
    }
};