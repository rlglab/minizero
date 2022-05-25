#include "killallgo_configuration.h"
#include "configuration.h"

namespace solver {

using namespace minizero;

int nn_value_size = 200;
bool use_rzone = false;

void setConfiguration(config::ConfigureLoader& cl)
{
    config::setConfiguration(cl);
    cl.addParameter("nn_value_size", nn_value_size, "", "Solver");
    cl.addParameter("use_rzone", use_rzone, "", "Solver");
}

} // namespace killallgo::solver