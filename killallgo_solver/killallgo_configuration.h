#pragma once

#include "configure_loader.h"

namespace solver {

extern int nn_value_size;
extern bool use_rzone;

void setConfiguration(minizero::config::ConfigureLoader& cl);

} // namespace killallgo::solver