# MiniZero

MiniZero is a zero-knowledge learning framework that supports AlphaZero, MuZero, Gumbel AlphaZero, and Gumbel MuZero algorithms.

If you use MiniZero for research, please consider citing our paper as follows:
```
@misc{wu2023minizero,
  title={MiniZero: Comparative Analysis of AlphaZero and MuZero on Go, Othello, and Atari Games}, 
  author={Ti-Rong Wu and Hung Guei and Po-Wei Huang and Pei-Chiun Peng and Ting Han Wei and Chung-Chin Shih and Yun-Jui Tsai},
  year={2023},
  eprint={2310.11305},
  archivePrefix={arXiv},
  primaryClass={cs.AI}
}
```

Outline
* [Overview](#Overview)
    * [Architecture](#Architecture)
    * [Results](#Results)
    * [Prerequisites](#Prerequisites)
* [Quick Start](#Quick-Start)
    * [Training](#Training)
    * [Evaluation](#Evaluation)
    * [Console](#Console)
* [Development](#Development)
* [References](#References)

## Overview

MiniZero utilizes zero-knowledge learning algorithms to train game-specific AI models.

It includes a variety of zero-knowledge learning algorithms:
* AlphaZero
* MuZero
* Gumbel AlphaZero
* Gumbel MuZero

It supports a variety of game environments:
* Go
* NoGo
* Killall-Go
* Gomoku / Outer-Open Gomoku
* Othello
* Hex
* TicTacToe
* Atari (57 games)

We are planning to add new algorithms, features, and more games in the future.

### Architecture

The MiniZero architecture comprises four components: a *server*, *self-play workers*, an *optimization worker*, and *data storage*.

![MiniZero Architecture](docs/imgs/minizero-architecture.svg)

<details>
<summary>Server</summary>

The server is the core component in MiniZero, controlling the training process and managing both the self-play and optimization workers.

In each iteration, the server first instructs all self-play workers to generate self-play games simultaneously using the latest network and collects game records from self-play workers. 
Once the server accumulates the necessary self-play games, it then stops the self-play workers and instructs the optimization worker to load the latest game records and start network updates.
After the network has been updated, the server starts the next iteration until the training reaches a predetermined maximum iteration.

</details>

<details>
<summary>Self-play worker</summary>

The self-play worker interacts with the environment to produce self-play games.

There may be multiple self-play workers. Each self-play worker maintains multiple MCTS instances to play multiple games simultaneously with batch GPU inferencing to improve efficiency.
Specifically, the self-play worker runs the selection for each MCTS to collect a batch of leaf nodes and then evaluates them through batch GPU inferencing.
Finished self-play games are sent to the server and forwarded to the data storage by the server.
    
</details>

<details>
<summary>Optimization worker</summary>

The optimization worker updates the network using collected self-play games.

Specifically, it loads self-play games from data storage and stores them into the replay buffer, and then updates the network over steps using data sampled from the replay buffer.
Generally, the number of optimized steps is proportional to the number of collected self-play games to prevent overfitting.
Finally, the updated networks are stored into the data storage.

</details>

<details>
<summary>Data storage</summary>

The data storage stores network files and self-play games.

Specifically, it uses the Network File System (NFS) for sharing data across different machines.
This is an implementation choice; a simpler file system can suffice if distributed computing is not employed.

</details>

### Results

The performance of each zero-knowledge learning algorithm on board games and Atari games are shown as follows, where α<sub>0</sub>, μ<sub>0</sub>, g-α<sub>0</sub>, and g-μ<sub>0</sub> represent AlphaZero, MuZero, Gumbel AlphaZero, and Gumbel MuZero, and $n$ represents simulation count.
Check [our paper](https://arxiv.org/abs/2310.11305) for more details.

Results on board games:

<img src="docs/imgs/minizero_go_9x9.svg" alt="Go 9x9" width="50%"><img src="docs/imgs/minizero_othello_8x8.svg" alt="Othello 8x8" width="50%">

Results on Atari games:

<img src="docs/imgs/minizero_atari.svg" alt="Atari" width="100%">

Trained game-playing AI models for these games will be released soon.

### Prerequisites

MiniZero requires a Linux platform with at least one NVIDIA GPU to operate.
To facilitate the use of MiniZero, a [container image](https://hub.docker.com/r/kds285/minizero) is pre-built to include all required packages. 
Thus, a container tool such as `docker` or `podman` is also required.

<details>
<summary>Show platform recommendations</summary>

* Modern CPU with at least 64G RAM
* NVIDIA GPU of GTX 1080 (VRAM 8G) or above
* Linux operating system, e.g., Ubuntu 22.04 LTS

</details>

<details>
<summary>Show tested platforms</summary>

|CPU|RAM|GPU|OS|
|---|---|---|--|
|Xeon Silver 4216 x2|256G|RTX A5000 x4|Ubuntu 20.04.6 LTS|
|Xeon Silver 4216 x2|128G|RTX 3080 Ti x4|Ubuntu 20.04.5 LTS|
|Xeon Silver 4216 x2|256G|RTX 3090 x4|Ubuntu 20.04.5 LTS|
|Xeon Silver 4210 x2|128G|RTX 3080 x4|Ubuntu 22.04 LTS|
|Xeon E5-2678 v3 x2|192G|GTX 1080 Ti x4|Ubuntu 20.04.5 LTS|
|Xeon E5-2698 v4 x2|128G|GTX 1080 Ti x1|Arch Linux LTS (5.15.90)|
|Core i9-7980XE|128G|GTX 1080 Ti x1|Arch Linux (6.5.6)|

</details>

## Quick Start

This section walks you through training AI models using zero-knowledge learning algorithms, evaluating trained AI models, and launching the console to interact with the AI.

First, clone this repository.

```bash
git clone git@github.com:rlglab/minizero.git
cd minizero # enter the cloned repository
```

Then, start the runtime environment using the container. 

```bash
scripts/start-container.sh # must have either podman or docker installed
```

Once a container starts successfully, its working folder should be located at `/workspace`.
You must execute all of the following commands inside the container.

### Training

To train 9x9 Go:
```bash
# AlphaZero with 200 simulations
tools/quick-run.sh train go az 300 -n go_9x9_az_n200 -conf_str env_board_size=9:actor_num_simulation=200

# Gumbel AlphaZero with 16 simulations
tools/quick-run.sh train go gaz 300 -n go_9x9_gaz_n16 -conf_str env_board_size=9:actor_num_simulation=16
```

To train Ms. Pac-Man:
```bash
# MuZero with 50 simulations
tools/quick-run.sh train atari mz 300 -n ms_pacman_mz_n50 -conf_str env_atari_name=ms_pacman:actor_num_simulation=50

# Gumbel MuZero with 18 simulations
tools/quick-run.sh train atari gmz 300 -n ms_pacman_gmz_n18 -conf_str env_atari_name=ms_pacman:actor_num_simulation=18
```

For more training details, please refer to [this instructions](docs/Training.md).

### Evaluation

To evaluate the strength growth during training:
```bash
# the strength growth for "go_9x9_az_n200"
tools/quick-run.sh self-eval go go_9x9_az_n200 -conf_str env_board_size=9:actor_num_simulation=800:actor_select_action_by_count=true:actor_select_action_by_softmax_count=false:actor_use_dirichlet_noise=false:actor_use_gumbel_noise=false
```

To compare the strengths between two trained AI models:
```bash
# the relative strengths between "go_9x9_az_n200" and "go_9x9_gaz_n16"
tools/quick-run.sh fight-eval go go_9x9_az_n200 go_9x9_gaz_n16 -conf_str env_board_size=9:actor_num_simulation=800:actor_select_action_by_count=true:actor_select_action_by_softmax_count=false:actor_use_dirichlet_noise=false:actor_use_gumbel_noise=false
```

Note that the evaluations is generated during training in Atari games.
Check `ms_pacman_mz_n50/analysis/*_Return.png` for the results.

For more evaluation details, please refer to [this instructions](docs/Evaluation.md).

### Console

To interact with a trained model using [Go Text Protocol (GTP)](http://www.lysator.liu.se/~gunnar/gtp/).
```bash
# play with the "go_9x9_az_n200" model
tools/quick-run.sh console go go_9x9_az_n200 -conf_str env_board_size=9:actor_num_simulation=800:actor_select_action_by_count=true:actor_select_action_by_softmax_count=false:actor_use_dirichlet_noise=false:actor_use_gumbel_noise=false
```

For more console details, please refer to [this instructions](docs/Console.md).

## Development

We are actively adding new algorithms, features, and games into MiniZero.

The following work-in-progress features will be available in future versions:
* Stochastic MuZero
* Sampled MuZero

We welcome developers to join the MiniZero community. 
For more development tips, please refer to [this instructions](docs/Development.md).

## References
- [MiniZero: Comparative Analysis of AlphaZero and MuZero on Go, Othello, and Atari Games](https://arxiv.org/abs/2310.11305)
- [Policy improvement by planning with Gumbel (Gumbel AlphaZero and Gumbel MuZero)](https://openreview.net/forum?id=bERaNdoegnO)
- [Mastering Atari, Go, chess and shogi by planning with a learned model (MuZero)](https://doi.org/10.1038/s41586-020-03051-4)
- [A general reinforcement learning algorithm that masters chess, shogi, and Go through self-play (AlphaZero)](https://doi.org/10.1126/science.aar6404)
- [Mastering the game of Go without human knowledge (AlphaGo Zero)](https://doi.org/10.1038/nature24270)
