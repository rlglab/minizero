## Setup

This section introduces how to obtain the project source code, how to launch the container, and how to build and run the program.
* [Clone the repository](#Clone-the-repository)
* [Launch the container](#Launch-the-container)
* [Build program](#Build-program)
* [Run program](#Run-program)

### Clone the repository

To clone the project repository from GitHub, run
```bash!
git clone git@github.com:rlglab/minizero.git
```

A successful clone produces a folder named `minizero` containing source code, scripts, and tools.

> **Note**
> For passwordless cloning, it is recommended to [set up an ssh key for GitHub](https://docs.github.com/en/authentication/connecting-to-github-with-ssh/adding-a-new-ssh-key-to-your-github-account?platform=linux&tool=webui).

### Launch the container

[A container image](https://hub.docker.com/r/kds285/minizero) is prebuilt to contain the necessary softwares for this project. 
All executables in this project, including scripts and tools, **MUST** run inside the container.

To launch a container instance, use the provided script as shown below.
```bash!
./scripts/start-container.sh
```

> **Note**
> Before the first launch (on each machine), the script will automatically download and cache the container image. Afterwards, the script will use the cached image to launch.
> 
> However, the cached image is not refreshed automatically. When the image is updated, you should manually download the image again (on each machine) by `podman pull kds285/minizero:latest`

### Build program

The `build.sh` script is designed for automated builds.

```bash!
./scripts/build.sh [GAME_TYPE] [BUILD_TYPE]
# E.g., ./scripts/build.sh tictactoe release
```
- `GAME_TYPE`:  the target game, e.g., `atari`,`go`,`gomoku`,`othello`,`tictactoe`.
    - Running different games requires different builds with corresponding `GAME_TYPE`.
    - For the full list of supported games, run `./scripts/build.sh -h`
    - If `GAME_TYPE` is not specified, the script will rebuild all existing builds.
- `BUILD_TYPE`: `release` (default) or `debug`.

After build, the executable is generated at `build/[GAME_TYPE]/minizero_[GAME_TYPE]`, e.g., `build/tictactoe/minizero_tictactoe`.

> **Note**
> The script runs `cmake` for setup and runs `make` for build.
> However, `cmake` only runs on the first build. After changing `cmake` files or adding source files, you may need to remove the existing `build` folder to run `cmake` again.

### Run program

The program defines several *modes* for different scenarios, specified by `-mode [MODE]`, e.g.,

```bash
./build/[GAME_TYPE]/minizero_[GAME_TYPE] [ARGUMENT]...
# E.g., ./build/tictactoe/minizero_tictactoe -mode env_test
# E.g., ./build/tictactoe/minizero_tictactoe -mode consle
```

- `-mode env_test`: play a game to test the game environment
- `-mode console`: interact with the program by GTP protocal (default mode)

> **Note**
> For the full list of supported modes, run the program with `-h`.
> When using the program for standard scenarios such as [training](Training.md) or [testing](Testing.md), you **DO NOT** need to run it directly. Instead, scripts are provided. See the related sections for more details.

The program configuration can be set using a *configuration file* or a *configuration string*, e.g.,
```bash
./build/[GAME_TYPE]/minizero_[GAME_TYPE] [ARGUMENT]...
# E.g., ./build/tictactoe/minizero_tictactoe -gen tictactoe.cfg
# E.g., ./build/tictactoe/minizero_tictactoe -conf_file tictactoe.cfg # run default console mode
# E.g., ./build/tictactoe/minizero_tictactoe -conf_file tictactoe.cfg -conf_str actor_num_simulation=100
```
- `-gen [CONF_FILE]`: generate a clean configuration file (`*.cfg`)
- `-conf_file [CONF_FILE]`: specify a configuration file (`*.cfg`)
    - A configuration file is a collection of `key=value` settings.
    - If no configuration is specified, the program will run with the built-in default. 
- `-conf_str [CONF_STR]`: set configuration via command line
    - The `CONF_STR` may contain multiple `key=value` pairs, seperated by the colon character.
    - The settings set by `-conf_str` are adopted with the highest priority.

> **Note**
> The arguments `-conf_file` and `-conf_str` can be set at the same time. For example,
> ```bash
> ./build/tictactoe/minizero_tictactoe -mode console -conf_file tictactoe.cfg -conf_str "nn_file_name=tictactoe_az/model/weight_iter_10000.pt:actor_num_simulation=100"
> ```
> In the example, the program is started using `console` mode.
> The two `-conf_str` settings, `nn_file_name` and `actor_num_simulation`, overwrite the same keys in the `tictactoe.cfg` (if present). Note that for settings neither specified by `-conf_str` nor in `tictactoe.cfg`, the built-in defaults are applied.
