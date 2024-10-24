#pragma once

#include "configuration.h"

#if AMAZONS
#include "amazons.h"
typedef minizero::env::amazons::AmazonsAction Action;
typedef minizero::env::amazons::AmazonsEnv Environment;
typedef minizero::env::amazons::AmazonsEnvLoader EnvironmentLoader;
#elif ATARI
#include "atari.h"
typedef minizero::env::atari::AtariAction Action;
typedef minizero::env::atari::AtariEnv Environment;
typedef minizero::env::atari::AtariEnvLoader EnvironmentLoader;
#elif BREAKTHROUGH
#include "breakthrough.h"
typedef minizero::env::breakthrough::BreakthroughAction Action;
typedef minizero::env::breakthrough::BreakthroughEnv Environment;
typedef minizero::env::breakthrough::BreakthroughEnvLoader EnvironmentLoader;
#elif CLOBBER
#include "clobber.h"
typedef minizero::env::clobber::ClobberAction Action;
typedef minizero::env::clobber::ClobberEnv Environment;
typedef minizero::env::clobber::ClobberEnvLoader EnvironmentLoader;
#elif CONHEX
#include "conhex.h"
typedef minizero::env::conhex::ConHexAction Action;
typedef minizero::env::conhex::ConHexEnv Environment;
typedef minizero::env::conhex::ConHexEnvLoader EnvironmentLoader;
#elif CONNECT6
#include "connect6.h"
typedef minizero::env::connect6::Connect6Action Action;
typedef minizero::env::connect6::Connect6Env Environment;
typedef minizero::env::connect6::Connect6EnvLoader EnvironmentLoader;
#elif DOTSANDBOXES
#include "dotsandboxes.h"
typedef minizero::env::dotsandboxes::DotsAndBoxesAction Action;
typedef minizero::env::dotsandboxes::DotsAndBoxesEnv Environment;
typedef minizero::env::dotsandboxes::DotsAndBoxesEnvLoader EnvironmentLoader;
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
#elif HAVANNAH
#include "havannah.h"
typedef minizero::env::havannah::HavannahAction Action;
typedef minizero::env::havannah::HavannahEnv Environment;
typedef minizero::env::havannah::HavannahEnvLoader EnvironmentLoader;
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
#elif LINESOFACTION
#include "linesofaction.h"
typedef minizero::env::linesofaction::LinesOfActionAction Action;
typedef minizero::env::linesofaction::LinesOfActionEnv Environment;
typedef minizero::env::linesofaction::LinesOfActionEnvLoader EnvironmentLoader;
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
#elif SANTORINI
#include "santorini.h"
typedef minizero::env::santorini::SantoriniAction Action;
typedef minizero::env::santorini::SantoriniEnv Environment;
typedef minizero::env::santorini::SantoriniEnvLoader EnvironmentLoader;
#elif SURAKARTA
#include "surakarta.h"
typedef minizero::env::surakarta::SurakartaAction Action;
typedef minizero::env::surakarta::SurakartaEnv Environment;
typedef minizero::env::surakarta::SurakartaEnvLoader EnvironmentLoader;
#else
#include "tictactoe.h"
typedef minizero::env::tictactoe::TicTacToeAction Action;
typedef minizero::env::tictactoe::TicTacToeEnv Environment;
typedef minizero::env::tictactoe::TicTacToeEnvLoader EnvironmentLoader;
#endif

namespace minizero::env {

inline void setUpEnv()
{
#if AMAZONS
    amazons::initialize();
#elif ATARI
    atari::initialize();
#elif BREAKTHROUGH
    breakthrough::initialize();
#elif GO
    go::initialize();
#elif KILLALLGO
    killallgo::initialize();
#elif LINESOFACTION
    linesofaction::initialize();
#elif NOGO
    nogo::initialize();
#endif

#if AMAZONS
    config::env_board_size = 10;
#elif ATARI
    config::learner_n_step_return = 10;
    config::zero_actor_intermediate_sequence_length = 200;
#elif BREAKTHROUGH
    config::env_board_size = 8;
#elif CLOBBER
    config::env_board_size = 10;
#elif CONNECT6
    config::env_board_size = 19;
#elif CONHEX
    config::env_board_size = 9;
#elif DOTSANDBOXES
    config::env_board_size = 9;
#elif GO
    config::env_board_size = 9;
#elif HAVANNAH
    config::env_board_size = 8;
#elif HEX
    config::env_board_size = 11;
#elif KILLALLGO
    config::env_board_size = 7;
#elif LINESOFACTION
    config::env_board_size = 8;
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
    config::learner_n_step_return = 10;
    config::zero_actor_intermediate_sequence_length = 200;
#elif RUBIKS
    config::env_board_size = 3;
#elif SANTORINI
    config::env_board_size = 5;
#elif SURAKARTA
    config::env_board_size = 6;
#endif
}

} // namespace minizero::env
