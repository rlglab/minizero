#pragma once

#if ATARI
#include "atari.h"
typedef minizero::env::atari::AtariAction Action;
typedef minizero::env::atari::AtariEnv Environment;
typedef minizero::env::atari::AtariEnvLoader EnvironmentLoader;
#elif CHESS
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
#elif HEX
#include "hex.h"
typedef minizero::env::hex::HexAction Action;
typedef minizero::env::hex::HexEnv Environment;
typedef minizero::env::hex::HexEnvLoader EnvironmentLoader;
#elif KILLALLGO
#include "killallgo.h"
typedef minizero::env::killallgo::KillAllGoAction Action;
typedef minizero::env::killallgo::KillAllGoEnv Environment;
typedef minizero::env::killallgo::KillAllGoEnvLoader EnvironmentLoader;
#elif OTHELLO
#include "othello.h"
typedef minizero::env::othello::OthelloAction Action;
typedef minizero::env::othello::OthelloEnv Environment;
typedef minizero::env::othello::OthelloEnvLoader EnvironmentLoader;
#elif PUZZLE2048
#include "puzzle2048.h"
typedef minizero::env::puzzle2048::Puzzle2048Action Action;
typedef minizero::env::puzzle2048::Puzzle2048Env Environment;
typedef minizero::env::puzzle2048::Puzzle2048EnvLoader EnvironmentLoader;
#else
#include "tictactoe.h"
typedef minizero::env::tictactoe::TicTacToeAction Action;
typedef minizero::env::tictactoe::TicTacToeEnv Environment;
typedef minizero::env::tictactoe::TicTacToeEnvLoader EnvironmentLoader;
#endif

namespace minizero::env {

inline void setUpEnv()
{
#if GO or KILLALLGO
    go::initialize();
#endif
}

} // namespace minizero::env
