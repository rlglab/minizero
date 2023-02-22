#pragma once

#include "alphazero_network.h"
#include "muzero_network.h"
#include "network.h"
#include <memory>
#include <string>

namespace minizero::network {

inline std::shared_ptr<Network> createNetwork(const std::string& nn_file_name, const int gpu_id)
{
    // TODO: how to speed up?
    Network base_network;
    base_network.loadModel(nn_file_name, -1);

    std::shared_ptr<Network> network;
    if (base_network.getNetworkTypeName() == "alphazero") {
        network = std::make_shared<AlphaZeroNetwork>();
        std::dynamic_pointer_cast<AlphaZeroNetwork>(network)->loadModel(nn_file_name, gpu_id);
    } else if (base_network.getNetworkTypeName() == "muzero" || base_network.getNetworkTypeName() == "muzero_atari") {
        network = std::make_shared<MuZeroNetwork>();
        std::dynamic_pointer_cast<MuZeroNetwork>(network)->loadModel(nn_file_name, gpu_id);
    } else {
        // should not be here
        assert(false);
    }

    return network;
}

} // namespace minizero::network
