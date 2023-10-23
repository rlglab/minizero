# Training

This document guides you in training the game-playing AI model.

## Use Quick-Run Script

Specify the target game, the zero algorithm, and the total iterations to start a training session.

```bash
tools/quick-run.sh train GAME_TYPE ALGORITHM END_ITER [OPTION]...
```

* `GAME_TYPE` sets the target game, e.g., `tictactoe`.
* `ALGORITHM` sets the zero algorithm, which should be one of `az`, `mz`, `gaz`, and `gmz`.
* `END_ITER` sets the total number of iterations for training, e.g., `100`.
* `OPTION` sets optional arguments, e.g., `-conf_str` sets additional configurations.

For detailed arguments and supported games, run `tools/quick-run.sh train -h`.

Sample commands:
```bash
# train a TicTacToe model using AlphaZero for 50 iterations
tools/quick-run.sh train tictactoe az 50

# train a 9x9 Go model using Gumbel AlphaZero with a simulation of 16 for 100 iterations
tools/quick-run.sh train go gaz 100 -conf_str env_board_size=9:actor_num_simulation=16

# train a Ms.Pac-Man model (an Atari game) using Gumbel MuZero for 200 iterations
tools/quick-run.sh train atari gmz 200 -conf_str env_atari_name=ms_pacman
```

</details>

## Launch Training Components Manually

Instead of using the quick-run script, you may start the training by manually launching each training component: a zero server, several self-play workers, and an optimization worker.

### Launch the zero server

The server manages the training session and should be launched before other components. To start a training session until a given end iteration, run
```bash!
scripts/zero-server.sh [GAME_TYPE] [CONF_FILE] [END_ITERATION] [OPTION]...
# E.g., scripts/zero-server.sh tictactoe tictactoe.cfg 100
```
* `CONF_FILE` specifies a configuration file.
* `END_ITERATION` sets the end iteration for training.
* `OPTION` are other optional arguments; run the script with `-h` for a list of supported options.
    * `-conf_str` sets configuration string for this server
    * `-n` sets the storage folder name
    * `-np` appends a prefix string to the default storage folder name
    * `-ns` appends a suffix string to the default storage folder name

> **Note**
> The server use port 9999 by default. When the port is occupied, you MUST use `-conf_str` to change the server port as shown below.
> ```bash
> scripts/zero-server.sh tictactoe tictactoe.cfg 100 -conf_str zero_server_port=11111
> ```

First, the script initialize a folder for storing training log and models. 
Based on the configuration, the folder name is automatically generated, e.g., `tictactoe_az_5bx256_n400-af37e1`.

Then, the program will print the current configuration and start printing training logs with time stamps. 
Once you have see this, it is ready to start other training components.

> **Note**
> If the script exits immediately without any warnings, it is most likely that the configuration file is incorrect, or the network model initialization is failed. 
> 1. Run the script with `bash -x` to let the script print all executed commands: `bash -x scripts/zero-server.sh ...`
> 2. The last executed command is the cause.
> 3. Run that command again without the script to reproduce the issue.

### Launch the zero worker

There are two types of workers, `sp` for *self-play* and `op` for *optimization*. 
To start a worker, run
```bash!
scripts/zero-worker.sh [GAME_TYPE] [SERVER] [SERVER_PORT] [sp|op] [OPTION]...
# E.g., scripts/zero-worker.sh tictactoe localhost 9999 sp
```
* `SERVER` and `SERVER_PORT` specify where to connect to the zero server.
* `sp` or `op` specifies the type of the worker.
* `OPTION` are other optional arguments; run the script with `-h` for a list of supported options.
    * `-conf_str` sets configuration string for this worker
    * `-g` sets the available GPUs for this worker (default all)
    * `-b` sets the batch size in `sp` worker (default 64)
    * `-c` sets the number of CPUs for each GPU in `sp` worker (default 4)

Both `sp` and `op` workers are required. You have to start each of them in different terminals.

For self-play, you should launch one `sp` worker instance per available GPU:
```bash!
# assume that GPU 0-3 are available, start four workers as follows.
scripts/zero-worker.sh tictactoe localhost 9999 sp -g 0
scripts/zero-worker.sh tictactoe localhost 9999 sp -g 1
scripts/zero-worker.sh tictactoe localhost 9999 sp -g 2
scripts/zero-worker.sh tictactoe localhost 9999 sp -g 3
```

For optimization, you should launch at most one `op` worker instance with all available GPUs:
```bash!
scripts/zero-worker.sh tictactoe localhost 9999 op # uses all GPUs by default
```

Note that workers can be hosted on different machines. 
Once you have successfully started a worker and connected the worker to a server, the server will print a connection message.

## Handle Training Results

The training results are stored in a folder named after the important training settings, e.g., `tictactoe_az_1bx256_n50-cb69d4`, which includes the following files:
* `analysis/`: the folder that contains figures of the training process.
    * `*_accuracy_policy.png`, `*_loss_policy.png`, and `*_loss_value.png`: training accuracy and loss
    * `*_Lengths.png`: self-play game lengths
    * `*_Returns.png`: self-play game returns
    * `*_Time.png`: elapsed training time
* `model/`: the folder that stores all network models produced by each optimization step.
    * `*.pkl`: include training step, parameters, optimizer, scheduler (use for training).
    * `*.pt`: model parameters (use for testing).
* `sgf/`: the folder that stores self-play games of each iteration.
    * `1.sgf`, `2.sgf`, ... for the 1^st^, the 2^nd^, ... iteration, respectively.
* `*.cfg`: the configurations for this training session.
* `Training.log`: the main training log.
* `Worker.log`: the worker connection log.
* `op.log`: the optimization worker log.

After the training, you can use the trained network models (saved inside `model/`) to run [evaluation](docs/Evaluation.md) or [console](docs/Console.md).

On the other hand, you can check self-play records (saved inside `sgf/`) by GoGui or by videos.

For board games, use [GoGui](https://github.com/Remi-Coulom/gogui) to view self-play records.
However, it is required to run `to-sgf.py` to convert the SGF format first. The `[OUTPUT_SGF]` can then be opened by GoGui.
```bash
tools/to-sgf.py -in_file [INPUT_SGF] -out_file [OUTPUT_SGF]
```

For Atari games, use `to-video.py` to convert the self-playing records into videos.
The records will be saved as `*.mp4` in `[OUTPUT_DIR]`.
```bash
tools/to-video.py -in_file [INPUT_SGF] -out_dir [OUTPUT_DIR]
```

</details>

## Miscellaneous Training Tips

### Use configuration files

For a better organization of configurations, it is recommended to use config files (`.cfg`).

First, use `-gen` flag with ordinary training arguments to copy the default settings to a config file.

```bash
# save the default AlphaZero training settings for TicTacToe to "tictactoe-train.cfg"
tools/quick-run.sh train tictactoe az 50 -gen tictactoe-train.cfg

# save the modified (by -conf_str) Gumbel AlphaZero training settings for Go to "go_gaz.cfg"
tools/quick-run.sh train go gaz 100 -conf_str env_board_size=9:actor_num_simulation=16 -gen go_gaz.cfg
```

You may edit the file to tune the configurations.

Then, run the script with ordinary arguments but set the `*.cfg` at the `ALGORITHM` argument to appply it. 

```bash
# use configurations specified in "tictactoe-train.cfg" for training a TicTacToe model
tools/quick-run.sh train tictactoe tictactoe-train.cfg 20

# use configurations specified in "go_gaz.cfg" and overwrite two keys using -conf_str for training a Go model
tools/quick-run.sh train go go_gaz.cfg 100 -conf_str env_board_size=7:nn_num_blocks=2
```

### Set training folder name

The folder name is automatically generated using the important training settings by default; however, it can be explicitly set by flag `-n` as follows.

```bash
# save results into folder "tictactoe_az_model"
tools/quick-run.sh train tictactoe az 50 -n tictactoe_az_model
```

Note that if the folder already exist, the program will prompt you to choose whether to retrain the model or to continue training. 
To train another model with the same settings, you should explicitly assign another folder name.

### Assign another server port

The training components use TCP connections for message exchange, in which the server uses port 9999 by default.
When the port is occupied, you MUST change the server port as shown below.

```bash
# use port 11111
tools/quick-run.sh train tictactoe az 50 -conf_str zero_server_port=11111
```

### Assign available GPUs for training

The program uses all available GPUs for training by default.
However, on platforms with multiple GPUs installed, you may explicitly assign available GPUs using the `-g` flag.

```bash
# use GPU 1
tools/quick-run.sh train tictactoe az 50 -g 1

# use GPUs 0, 1, 2, and 3
tools/quick-run.sh train tictactoe az 50 -g 0123
```

### Continue a previous training session

Just run the script with a larger `END_ITER` and the previous training folder (by `-n`) to continue.

```bash
# continue the training of tictactoe_az_1bx256_n50-cb69d4, use AlphaZero to reach 50th iteration
tools/quick-run.sh train tictactoe az 50 -n tictactoe_az_1bx256_n50-cb69d4

# continue the training of go_9x9_gaz_1bx256_n16-cb69d4, use go_gaz.cfg with an overwritten config to reach 100th iteration
tools/quick-run.sh train go go_gaz.cfg 100 -n go_9x9_gaz_1bx256_n16-cb69d4 -conf_str actor_num_simulation=32
```
