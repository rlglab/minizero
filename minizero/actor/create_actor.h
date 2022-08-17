#pragma once

#include "base_actor.h"
#include "network.h"

namespace minizero::actor {

using namespace minizero;

std::shared_ptr<actor::BaseActor> createActor(long long tree_node_size, const std::shared_ptr<network::Network>& network);

} // namespace minizero::actor