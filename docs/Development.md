# Development

For execution efficiency, the core components (e.g., MCTS) are written in C++, while NN-related components are written in Python using the PyTorch library. To integrate C++ with Python, Pybind is adopted.

The following are development tips.

## Build Program

The `tools/quick-run.sh` script used in the [Quick Start](Training.md#Use-Quick-Run-Script) always automatically builds program when necessary.
However, to build program on demand, use `scripts/build.sh` as follows.

```bash!
scripts/build.sh [GAME_TYPE] [BUILD_TYPE]
# E.g., scripts/build.sh tictactoe release
```
* `GAME_TYPE`:  the target game, e.g., `atari`,`go`,`gomoku`,`othello`,`tictactoe`.
    * Running different games requires different builds with corresponding `GAME_TYPE`.
    * For the full list of supported games, run `scripts/build.sh -h`
    * If `GAME_TYPE` is not specified, the script will rebuild all existing builds.
* `BUILD_TYPE`: `release` (default) or `debug`.
    * To change a `release` build to a `debug` build, you have to manually remove `build/[GAME_TYPE]` folder and restart the build script; and vice versa.

The script runs `cmake` for setup and runs `make` for build.
After build, the executable is generated at `build/[GAME_TYPE]/minizero_[GAME_TYPE]`, e.g., `build/tictactoe/minizero_tictactoe`.

For modifying existing source files, simply run the build script again for an increasemental build.
However, for adding new source files, the `build/[GAME_TYPE]` folder must be removed before running the build script to let `cmake` be triggered again.

## Launch Program

For development, this subsection introduces how to launch the program directly instead of using the quick-run script.

### Program mode

The MiniZero program defines *modes* for different scenarios, specified by `-mode [MODE]`, e.g.,

```bash
build/[GAME_TYPE]/minizero_[GAME_TYPE] [OPTION]...
# E.g., ./build/tictactoe/minizero_tictactoe -mode env_test
# E.g., ./build/tictactoe/minizero_tictactoe -mode console
```

For the full list of supported modes, run the program with `-h`.

When using the program for standard scenarios such as training or testing, you **DO NOT** need to run the program directly. 
Instead, scripts are provided. See the [related sections](Training.md) for more details.

### Program configuration

The program configuration can be set using a *configuration file* or a *configuration string*, e.g.,
```bash
build/[GAME_TYPE]/minizero_[GAME_TYPE] [OPTION]...
# E.g., build/tictactoe/minizero_tictactoe -gen tictactoe.cfg
# E.g., build/tictactoe/minizero_tictactoe -conf_file tictactoe.cfg # run default console mode
# E.g., build/tictactoe/minizero_tictactoe -conf_file tictactoe.cfg -conf_str actor_num_simulation=100
```
* `-gen [CONF_FILE]`: generate a clean configuration file (`*.cfg`)
* `-conf_file [CONF_FILE]`: specify a configuration file (`*.cfg`)
    * A configuration file is a collection of `key=value` settings.
    * If no configuration is specified, the program will run with the built-in default. 
* `-conf_str [CONF_STR]`: set configuration via command line
    * The `CONF_STR` may contain multiple `key=value` pairs, seperated by the colon character.
    * The settings set by `-conf_str` are adopted with the highest priority.

> **Note**
> The arguments `-conf_file` and `-conf_str` can be set at the same time. For example,
> ```bash
> build/go/minizero_go -mode console -conf_file go.cfg -conf_str "nn_file_name=go_az/model/weight_iter_10000.pt:actor_num_simulation=100"
> ```
> In the example, the program is started using `console` mode.
> The two `-conf_str` settings, `nn_file_name` and `actor_num_simulation`, overwrite the same keys in the `go.cfg` (if present). Note that for settings neither specified by `-conf_str` nor in `go.cfg`, the built-in defaults are applied.

### Add a New Game

MiniZero has the ability to add new games, especially board games, which can be easily added.

Follow the below steps to add a new game.

1. Implement the environment. 
   Create a new folder under `environment/` for the new environment.
   Inherit `BaseEnv` class in [`minizero/environment/base/base_env.h`](minizero/environment/base/base_env.h).
   You may refer to the implementation of `tictactoe`. For more details about environment APIs, please check [`minizero/environment/base/base_env.h`](minizero/environment/base/base_env.h).
2. Add miscellaneous requirements.
   Add new `typedef` and config defaults in [`minizero/environment/environment.h`](minizero/environment/environment.h).
   Setup CMake in [`minizero/environment/CMakeLists.txt`](minizero/environment/CMakeLists.txt).
   Add environment name in [`scripts/build.sh`](scripts/build.sh).
3. Compile the program and run environment test.
   ```bash
   build/[NEW_GAME]/minizero_[NEW_GAME] -mode env_test
   ```

### Add a New Configuration

New configurations are often required when adding new games or developing new algorithms.

Follow the below steps to add a new configuration named `new_int_config`.

1. Add the key and default value.
   Declare the configuration in [`minizero/config/configuration.h`](minizero/config/configuration.h) and [`minizero/config/configuration.cpp`](minizero/config/configuration.cpp).
   MiniZero currently supports for configuration types: `int`, `float`, `bool`, and `std::string`.
   You have to modify three places, following the style of existing configurations.
   If the configuration has game-specific default, also edit `setUpEnv` in [`minizero/environment/environment.h`](minizero/environment/environment.h).
2. Include the header to use the new configuration. For example,
   ```cpp
   #include "configuration.h"
   // ... SKIP ...
   int new_int_config = config::new_int_config; // access new config
   ```
3. Edit the config file (`*.cfg`) or use `-conf_str` to execute MiniZero with it. For example,
   ```bash
   build/game/minizero_game ... -conf_str new_int_config=100
   ```

### Add a New Program Mode

MiniZero supports several program mode, e.g., `zero_server`, `console`, `env_test`.
When developing tools, new program modes are indispensable.

Follow the below steps to add a new program mode named `new_mode`.

1. Add functions for the new mode.
   Modify [`minizero/console/mode_handler.h`](minizero/console/mode_handler.h) and [`minizero/console/mode_handler.cpp`](minizero/console/mode_handler.cpp), following the style of existing modes.
   You may trace the implementations of existing modes declared in `minizero/console/mode_handler.cpp::RegisterFunction`.
2. Launch the MiniZero program with the new mode. For example,
   ```bash
   build/game/minizero_game -mode new_mode ...
   ```
