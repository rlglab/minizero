#pragma once

#if CHESS
#include "chess.h"
typedef minizero::env::chess::ChessAction Action;
typedef minizero::env::chess::ChessEnv Environment;
typedef minizero::env::chess::ChessEnvLoader EnvironmentLoader;
#elif GO
#include "go.h"
typedef minizero::env::go::GoAction Action;
typedef minizero::env::go::GoEnv Environment;
typedef minizero::env::go::GoEnvLoader EnvironmentLoader;
#elif GOMOKU
#include "gomoku.h"
typedef minizero::env::gomoku::GomokuAction Action;
typedef minizero::env::gomoku::GomokuEnv Environment;
typedef minizero::env::gomoku::GomokuEnvLoader EnvironmentLoader;
#elif OTHELLO
#include "othello.h"
typedef minizero::env::othello::OthelloAction Action;
typedef minizero::env::othello::OthelloEnv Environment;
typedef minizero::env::othello::OthelloEnvLoader EnvironmentLoader;
#elif KILLALLGO
#include "killallgo.h"
typedef minizero::env::killallgo::KillAllGoAction Action;
typedef minizero::env::killallgo::KillAllGoEnv Environment;
typedef minizero::env::killallgo::KillAllGoEnvLoader EnvironmentLoader;
#else
#include "tictactoe.h"
typedef minizero::env::tictactoe::TicTacToeAction Action;
typedef minizero::env::tictactoe::TicTacToeEnv Environment;
typedef minizero::env::tictactoe::TicTacToeEnvLoader EnvironmentLoader;
#endif

namespace minizero::env {

void setUpEnv();

} // namespace minizero::env