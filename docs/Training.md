## Training

This section introduces the procedure to train game-playing model for the specific game with Zero-based algorithms, including *AlphaZero*, *MuZero*, *Gumbel AlphaZero*, and *Gumbel MuZero*.
* [Generate configuration](#Generate-configuration)
    * [AlphaZero vs MuZero](#AlphaZero-vs-MuZero)
    * [Non-Gumbel vs Gumbel](#Non-Gumbel-vs-Gumbel)
* [Launch training components](#Launch-training-components)
	* [Launch the zero server](#Launch-the-zero-server)
	* [Launch the zero worker](#Launch-the-zero-worker)
* [Handle training result](#Handle-training-result)

In the following example, we use tictactoe as an example, to instruct how to train the model.
Before following the instructions below, you should build the executable first, check subsection [Build program](Setup.md#Build-program) for instructions.

### Generate configuration

In order to use the target training algorithm, you must modify the configuration accordingly.
It is recommended to [generate a clean configuration file](Setup.md#Run-program) and modify the required settings.

#### AlphaZero vs MuZero

Important settings for AlphaZero and MuZero are listed below.
```
# Actor
actor_num_simulation=400
actor_use_dirichlet_noise=true
actor_dirichlet_noise_alpha=0.03
actor_use_gumbel=false
actor_use_gumbel_noise=false

# Zero
zero_num_games_per_iteration=2000

# Learner
learner_training_step=200
learner_batch_size=1024

# Network
nn_num_hidden_channels=256
nn_num_blocks=5
nn_type_name=alphazero # or muzero
```

Note that only the most important settings are introduced here.
You may need to modify other unmentioned keys in different scenarios.
> **Note**
> The explanation of settings is attached in the generated configuration file. Check them yourself and revise the configuration carefully.

#### Non-Gumbel vs Gumbel

To apply the [Gumbel mechanism](https://openreview.net/forum?id=bERaNdoegnO), based on either AlphaZero or MuZero configuration, and further modify the keys as follows.
```
# Actor
actor_num_simulation=32
actor_use_dirichlet_noise=false
actor_use_gumbel=true
actor_use_gumbel_noise=true
```

### Launch training components

The training requires several components: a *zero server* and several *zero workers*, in which the workers run *self-play* and *optimization*.

#### Launch the zero server

The server manages the training session and should be launched before other components. To start a training session until a given end iteration, run
```bash!
./scripts/zero-server.sh [GAME_TYPE] [CONF_FILE] [END_ITERATION] [OPTION]...
# E.g., ./scripts/zero-server.sh tictactoe tictactoe.cfg 100
```
- `CONF_FILE` specifies a configuration file.
- `END_ITERATION` sets the end iteration for training.
- `OPTION` are other optional arguments; run the script with `-h` for a list of supported options.
    - `-conf_str` sets configuration string for this server
    - `-n` sets the storage folder name
    - `-np` appends a prefix string to the default storage folder name
    - `-ns` appends a suffix string to the default storage folder name

> **Note**
> The server use port 9999 by default. When the port is occupied, you MUST use `-conf_str` to change the server port as shown below.
> ```bash
> ./scripts/zero-server.sh tictactoe tictactoe.cfg 100 -conf_str zero_server_port=11111
> ```

First, the script initialize a folder for storing training log and models. Based on the configuration, the folder name is automatically generated, e.g., `tictactoe_az_5bx256_n400-af37e1`.

> **Note**
> If the folder already exist, the script will prompt you to choose whether to retrain the model or to continue training.
> 
> To train another model with the same settings, you should explicitly let the script use another folder name, e.g.,
> `./scripts/zero-server.sh tictactoe tictactoe.cfg 100 -n tictactoe_test`
>
> To continue training, you should specify an end iteration greater than the model was trained (otherwise, the script will end immediately) and use `-n` to specify the folder, e.g.,
> `./scripts/zero-server.sh tictactoe tictactoe.cfg 200 -n tictactoe_az_5bx256_n400-af37e1`

Then, the program will print the current configuration and start printing training logs with time stamps. Once you have see this, it is ready to start other training components.

> **Note**
> If the script exits immediately without any warnings, it is most likely that the configuration file is incorrect. Run the program directly with `-conf_file` to see what is misconfigured. E.g., `./build/tictactoe/minizero_tictactoe -conf_file tictactoe.cfg`

#### Launch the zero worker

There are two types of workers, `sp` for *self-play* and `op` for *optimization*. To start a worker, run
```bash!
./scripts/zero-worker.sh [GAME_TYPE] [SERVER] [SERVER_PORT] [sp|op] [OPTION]...
# E.g., ./scripts/zero-worker.sh tictactoe localhost 9999 sp
```
- `SERVER` and `SERVER_PORT` specify where to connect to the zero server.
> **Warning**
> Check the `SERVER_PORT` carefully before starting workers; it must be the same as the `zero_server_port` setting displayed by the launched zero server.
> 
- `sp` or `op` specifies the type of the worker.
- `OPTION` are other optional arguments; run the script with `-h` for a list of supported options.
    - `-conf_str` sets configuration string for this worker
    - `-g` sets the available GPUs for this worker (default all)
    - `-b` sets the batch size in `sp` worker (default 64)
    - `-c` sets the number of CPUs for each GPU in `sp` worker (default 4)

Both `sp` and `op` workers are required. You have to start each of them in different terminals.

For self-play, you should launch one `sp` worker instance per available GPU:
```bash!
# assume that GPU 0-3 are available, start four workers as follows.
./scripts/zero-worker.sh tictactoe localhost 9999 sp -g 0
./scripts/zero-worker.sh tictactoe localhost 9999 sp -g 1
./scripts/zero-worker.sh tictactoe localhost 9999 sp -g 2
./scripts/zero-worker.sh tictactoe localhost 9999 sp -g 3
```

For optimization, you should launch at most one `op` worker instance with all available GPUs:
```bash!
./scripts/zero-worker.sh tictactoe localhost 9999 op # uses all GPUs by default
```

Note that workers can be hosted on different machines. Once you have successfully started a worker and connected the worker to a server, the server will print a connection message.

### Handle training result

The training result and the log files are stored in the folder created by the zero server, e.g., `tictactoe_az_5bx256_n400-af37e1`.
- `model/`: a directory that contains  models for each iteration
    - `*.pkl`:  include training step, parameters, optimizer, scheduler (use for training)
    - `*.pt`:  model parameters (use for testing)
- `sgf/`: a directory that contains game records for each iteration
    - `1.sgf, 2.sgf, ...`
- `[TRAIN_DIR].cfg`: the configure file for training, named the same as the storage folder
- `op.log`: information of optimizer
    - loss_value, loss_policy, accuracy_policy, etc.
- `Training.log`: training log with timestamp for each iteration
    - self-play game lengths, game returns, etc.
- `Worker.log`: connection and disconnection timestamp of workers

After training, you may visualize the training process from `Training.log` and `op.log`. A script is provided to plot the curves of accuracy, loss, return, and other metrics. Run it as follows.
```bash
python tools/analysis.py -d [TRAIN_DIR] # check [TRAIN_DIR]/analysis/ for images
```

In addition, to open the game records in `sgf/` by [GoGui](https://github.com/Remi-Coulom/gogui), you should use `tools/tosgf.py` to convert them to general SGF files as follows.
```bash
python tools/tosgf.py -i [FIN_NAME] -o [FOUT_NAME]
```

Furthermore, you may follow [these instructions](Testing.md) to further analyze the log files and run testing to assess model performance.
