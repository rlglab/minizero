#include "environment.h"

namespace minizero::env {

void SetUpEnv()
{
#if GO
    go::Initialize();
#endif
}

} // namespace minizero::env