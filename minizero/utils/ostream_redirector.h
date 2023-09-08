#pragma once
#include <fstream>
#include <iostream>
#include <map>
#include <utility>

namespace minizero::utils {

class OstreamRedirector {
public:
    OstreamRedirector(std::ostream& src, std::ostream& dst) : src(src), sbuf(src.rdbuf(dst.rdbuf())) {}
    ~OstreamRedirector() { src.rdbuf(sbuf); }

public:
    static bool silence(std::ostream& src, bool silence = true)
    {
        static std::map<std::ostream*, std::pair<std::ostream*, OstreamRedirector*>> redirects;
        auto it = redirects.find(&src);
        if (silence && it == redirects.end()) {
            std::ostream* nullout = new std::ofstream;
            OstreamRedirector* redirect = new OstreamRedirector(src, *nullout);
            redirects.insert({&src, std::make_pair(nullout, redirect)});
            return true;
        } else if (!silence && it != redirects.end()) {
            std::ostream* nullout = it->second.first;
            OstreamRedirector* redirect = it->second.second;
            delete redirect;
            delete nullout;
            redirects.erase(it);
            return true;
        }
        return false;
    }

private:
    std::ostream& src;
    std::streambuf* const sbuf;
};

} // namespace minizero::utils
