#pragma once

#include "base_actor.h"
#include "configuration.h"
#include "gumbel_zero_actor.h"
#include "zero_actor.h"

namespace minizero::actor {

inline std::shared_ptr<actor::BaseActor> createActor(long long tree_node_size, const std::shared_ptr<network::Network>& network)
{
    if (config::actor_use_gumbel) {
        auto actor = std::make_shared<GumbelZeroActor>(tree_node_size);
        actor->setNetwork(network);
        actor->reset();
        return actor;
    } else {
        auto actor = std::make_shared<ZeroActor>(tree_node_size);
        actor->setNetwork(network);
        actor->reset();
        return actor;
    }

    assert(false);
    return nullptr;
}

} // namespace minizero::actor
