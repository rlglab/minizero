#pragma once

#if GO
#include "go.h"
typedef minizero::env::go::GoAction Action;
typedef minizero::env::go::GoEnv Environment;
typedef minizero::env::go::GoEnvLoader EnvironmentLoader;
#elif OTHELLO
#include "othello.h"
typedef minizero::env::othello::OthelloAction Action;
typedef minizero::env::othello::OthelloEnv Environment;
typedef minizero::env::othello::OthelloEnvLoader EnvironmentLoader;
#else
#include "tietactoe.h"
typedef minizero::env::tietactoe::TieTacToeAction Action;
typedef minizero::env::tietactoe::TieTacToeEnv Environment;
typedef minizero::env::tietactoe::TieTacToeEnvLoader EnvironmentLoader;
#endif

namespace minizero::env {

void SetUpEnv();

} // namespace minizero::env