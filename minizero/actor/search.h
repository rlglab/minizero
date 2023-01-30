#pragma once

namespace minizero::actor {

class Search {
public:
    Search() {}
    virtual ~Search() = default;

    virtual void reset() = 0;
};

} // namespace minizero::actor
