#pragma once

#include "configuration.h"

#if ATARI
#include "atari.h"
typedef minizero::env::atari::AtariAction Action;
typedef minizero::env::atari::AtariEnv Environment;
typedef minizero::env::atari::AtariEnvLoader EnvironmentLoader;
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
#elif NOGO
#include "nogo.h"
typedef minizero::env::nogo::NoGoAction Action;
typedef minizero::env::nogo::NoGoEnv Environment;
typedef minizero::env::nogo::NoGoEnvLoader EnvironmentLoader;
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
#elif RUBIKS
#include "rubiks.h"
typedef minizero::env::rubiks::RubiksAction Action;
typedef minizero::env::rubiks::RubiksEnv Environment;
typedef minizero::env::rubiks::RubiksEnvLoader EnvironmentLoader;
#else
#include "tictactoe.h"
typedef minizero::env::tictactoe::TicTacToeAction Action;
typedef minizero::env::tictactoe::TicTacToeEnv Environment;
typedef minizero::env::tictactoe::TicTacToeEnvLoader EnvironmentLoader;
#endif

namespace minizero::env {

inline void setUpEnv()
{
#if GO or KILLALLGO or NOGO
    go::initialize();
#endif

#if ATARI or PUZZLE2048
    config::learner_n_step_return = 10;
    config::zero_actor_intermediate_sequence_length = 200;
#endif

#if GO
    config::env_board_size = 9;
#elif HEX
    config::env_board_size = 11;
#elif KILLALLGO
    config::env_board_size = 7;
#elif OTHELLO
    config::env_board_size = 8;
#elif GOMOKU
    config::env_board_size = 15;
#elif CHESS
    config::env_board_size = 8;
#elif NOGO
    config::env_board_size = 9;
#elif TICTACTOE
    config::env_board_size = 3;
#elif PUZZLE2048
    config::env_board_size = 4;
#elif RUBIKS
    config::env_board_size = 3;
#endif
}

} // namespace minizero::env
