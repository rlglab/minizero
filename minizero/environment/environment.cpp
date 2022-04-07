#include "environment.h"

namespace minizero::env {

void setUpEnv()
{
#if GO or KILLALLGO
    go::initialize();
#endif
}

} // namespace minizero::env