#pragma once

#include "network.h"

namespace minizero::network {

std::shared_ptr<Network> createNetwork(const std::string& nn_file_name, const int gpu_id);

} // namespace minizero::network
