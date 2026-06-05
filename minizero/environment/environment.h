#pragma once

#include "configuration.h"

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
#include "tictactoe.h"
typedef minizero::env::tictactoe::TicTacToeAction Action;
typedef minizero::env::tictactoe::TicTacToeEnv Environment;
typedef minizero::env::tictactoe::TicTacToeEnvLoader EnvironmentLoader;
#endif

namespace minizero::env {

inline void setUpEnv()
{
    Environment::setUpEnv();
}

} // namespace minizero::env
