# Development

We are actively adding new algorithms, features, and games into MiniZero.

The following work-in-progress features will be available in future versions:
* Stochastic MuZero
* Sampled MuZero

We welcome developers to join the MiniZero community.
For more development tips, please refer to the following instructions.

## Directory Structure

* `docker/`: Dockerfile for building the training environment.
* `docs/`: Documentation.
* `minizero/`: Core implementation of MiniZero.
    * `actor/`: MCTS and actor logic.
    * `config/`: Configuration management.
    * `console/`: GTP console mode.
    * `environment/`: Game environment implementations.
    * `learner/`: Data loading and training logic.
    * `network/`: Neural network architectures.
    * `utils/`: Common utilities.
    * `zero/`: Zero-knowledge learning server.
* `scripts/`: Scripts for building and running.
* `tools/`: Utility tools for analysis and evaluation.

## Coding Style

We follow the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) and use `clang-format` for C++ code and `autopep8` for Python code.

## Adding a New Game

To add a new game environment, you need to:
1. Implement the game logic in `minizero/environment/`.
2. Inherit from `BaseBoardEnv` and `BaseBoardEnvLoader` (if it's a board game).
3. Register the new game in `minizero/environment/environment.h` and `minizero/environment/CMakeLists.txt`.
4. Add game-specific configurations in `minizero/config/configuration.cpp` and `minizero/config/configuration.h`.

## Building

MiniZero uses CMake for build management. You can use the `scripts/build.sh` script to build the executable for a specific game.

```bash
scripts/build.sh GAME_TYPE [BUILD_TYPE]
```

* `GAME_TYPE`: the target game, e.g., `go`, `othello`, `tictactoe`.
* `BUILD_TYPE`: `release` (default) or `debug`.
