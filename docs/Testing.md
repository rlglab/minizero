## Testing

This section introduces how to evaluate the performance of trained models.
Board games and Atari games use different testing methods.
* [Board games](#Board-games)
	* [Self evalutation](#Self-evalutation)
	* [Fight evaluation (two models)](#Fight-evaluation-two-models)
	* [Analyze evaluation results](#Analyze-evaluation-results)
* [Atari games](#Atari-games)
	* [Analyze evaluation results](#Analyze-evaluation-results)

### Board games

To run testing for board games, you should modify the configuration to get the best results from the program. Important settings are listed below.
```
# Actor
actor_select_action_by_count=true
actor_select_action_by_softmax_count=false
actor_use_dirichlet_noise=false
actor_use_gumbel=false
actor_use_gumbel_noise=false
```
> **Note**
> Modifying these settings disables the noise added during training so that the best action candidate is always selected.

#### Self evalutation

Self evalutation evaluates the relative strengths between different iterations of trained model.
```bash
./tools/self-eval.sh [GAME] [FOLDER] [CONFIG] [INTERVAL] [GAME_NUM] [OPTION]...
# E.g., ./tools/self-eval.sh tictactoe tictactoe_az_5bx256_n400-af37e1 tictactoe_play.cfg 10 100 -b 3
```
- `GAME`: the target game type
- `FOLDER`: The folder contains subfolder `model/`
- `CONFIG`: The configure file
- `INTERVAL`: The interval between the fighting models in the folder, e.g. 10 means fight the current model with the next 10th model in the folder
- `GAME_NUM`: How many games to play for each pair
- `OPTION` are other optional arguments; run the script with `-h` for a list of supported options.
    - `-g, --gpu` : Assign available GPUs for worker (default = all), e.g. `-g 0123`
    - `--num_threads` : Number of threads to play games (default = 2)
    - `-s` : Start from which `.pt` model file in the folder (default 0, the first `.pt` model file in the folder)
    - `-b` : Board Size
    - `-d` : Result Folder Name (default = `self_eval`)

The evaluation results are stored in `[FOLDER]/[RESLUT_FOLDER_NAME]`, in which game records are named by `1000_vs_0`, `2000_vs_1000`, ...

#### Fight evaluation (two models)

Fight evaluation evaluates the relative strengths between same iterations of two trained models.
```bash
./tools/fight-eval.sh [GAME] [FOLDER1] [FOLDER2] [CONFIG] [INTERVAL] [GAME_NUM] [OPTION]...
# E.g., ./tools/fight-eval.sh tictactoe tictactoe_az_5bx256_n400-af37e1 tictactoe_mz_5bx256_n32-af37e1 tictactoe_play.cfg 10 100 -d az_vs_mz_eval -b 3
```
- `GAME`: the target game type
- `FOLDER1` & `FOLDER2`: The folders contains subfolder `model/`
- `CONFIG`: The configure file
- `INTERVAL`: The interval between the fighting models in the folder, e.g. fight the next 10th model in `FOLDER1` with the same name model in `FOLDER2`
- `GAME_NUM`: How many games to play for each pair
- `OPTION` are other optional arguments; run the script with `-h` for a list of supported options.
    - `-g, --gpu` : Assign available GPUs for worker (default = all), e.g. `-g 0123`
    - `--num_threads` : Number of threads to play games (default = 2)
    - `-s` : Start from which `.pt` model file in Folder1 (default 0, the first `.pt` model file in the Folder1)
    - `-f` : The configure file of `FOLDER2`, if this option is not specified, `FOLDER2` use the configuration specified in `CONFIG`
    - `-b` : Board Size
    - `-d` : Result Folder Name (default = `[FOLDER1]_vs_[FOLDER2]_eval`)

The evaluation results are stored in `[FOLDER1]/[RESULT_FOLDER_NAME]`, in which game records are named by `1000`, `2000`, ...

#### Analyze evaluation results

A script is provided to analyze self evaluation and fight evaluation results.
```bash
python tools/eval.py -d [EVAL_FOLDER]
# E.g., python tools/eval.py -d tictactoe_az_5bx256_n400-af37e1/self_eval
# E.g., python tools/eval.py -d tictactoe_az_5bx256_n400-af37e1/az_vs_mz_eval -e tictactoe_az_5bx256_n400-af37e1/self_eval/elo.csv
```
- `-d, --dir` : the evaluation folder
- `-o, --out` : the file to save the analyze result (default = `[EVAL_FOLDER]/elo.csv`)
- `-e, --elo` : the elo of player 1 for computing the relative elo rating of player2 in fight-eval
- `-w, --white` : the flag that outputs the second player's win rate
- `--plot` : plot elo curve
- `--step` : training step, use for dividing the value of x-axis in the curve


### Atari games

For Atari games, there is usually no need for run explicit testing. In most cases, the latest tarining scores can be considered as the testing result.

To run testing for Atari games, you may modify the configuration for performance as for board games. A script is provided to evaluate the playing score of different iterations of a model.
```bash
./tools/atari-eval.sh [GAME] [FOLDER] [CONFIG] [INTERVAL] [GAME_NUM] [OPTION]...
# E.g., ./tools/atari-eval.sh ms_pacman atari_ms_pacman_mz_1bx256_n50-3b3761 atari_play.cfg 10 100 -c 8 -d eval_mz_n50 -s 500 -g 0
```
- `GAME`: the target Atari game name (e.g. ms_pacman)
- `FOLDER`: The folder contains the subfolder `model/`
- `CONFIG`: The configure file
- `INTERVAL`: The interval between the models in the folder, e.g. 10 means fight the current model with the next 10th model in the folder
- `GAME_NUM`: How many games to play for each model
- `OPTION` are other optional arguments; run the script with `-h` for a list of supported options.
    - `-g, --gpu` : Assign available GPUs for worker (default = all), e.g. `-g 0123`
    - `-s` : Start from which `.pt` model file in the folder (default 0, the first `.pt` model file in the folder)
    - `-d` : Assign Result Folder Name (default = `eval`)
    - `-b, --batch_size` : Assign the batch size in self-play worker (default = 32)
    - `-c, --cpu_thread_per_gpu` : Assign the number of CPUs for each GPU in self-play worker (default = 4)
    - `--sp_executable_file` : Assign the path for self-play executable file
    - `-conf_str` : Overwrite configuration file


The evaluation results are stored in `[FOLDER]/[RESULT_FOLDER_NAME]`, in which game records are named by `1000.sgf`, `2000.sgf`, ...

#### Analyze evaluation results

A script is provided to analyze Atari evaluation results.
```bash!
python tools/atari-eval.py -d [EVAL_FOLDER]
# E.g., python tools/atari-eval.py -d atari_ms_pacman_mz_1bx256_n50-3b3761/eval
```
- `-d, --dir` : the evaluation folder
- `-o, --out` : the file to save the analyze result (default = `[FOLDER]/reward.csv and [FOLDER]/reward.txt`)
- `--plot` : plot return curve

