#pragma once

#include "base_actor.h"
#include "configuration.h"
#include "zero_actor.h"
#include <memory>

namespace minizero::actor {

inline std::shared_ptr<actor::BaseActor> createActor(uint64_t tree_node_size, const std::shared_ptr<network::Network>& network)
{
    auto actor = std::make_shared<ZeroActor>(tree_node_size);
    actor->setNetwork(network);
    actor->reset();
    return actor;

    assert(false);
    return nullptr;
}

} // namespace minizero::actor
