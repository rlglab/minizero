# Evaluation

MiniZero currently supports two evaluation methods to evaluate program strength: [self-evaluation](#Self-Evaluation), and [fight-evaluation](#Fight-Evaluation).

## Self-Evaluation

Self-evaluation evaluates the relative strengths between different iterations in a training session, i.e., it evaluates whether a network model is continuously improving during traing.

```bash
tools/quick-run.sh self-eval GAME_TYPE FOLDER [CONF_FILE] [INTERVAL] [GAMENUM] [OPTION]...
```

* `GAME_TYPE` sets the target game, e.g., `tictactoe`.
* `FOLDER` sets the folder to be evaluated, which should contain the `model/` subfolder.
* `CONF_FILE` sets the config file for evaluation.
* `INTERVAL` sets the iteration interval between each model pair to be evaluated, e.g. `10` indicates to pair the 0<sup>th</sup> and the 10<sup>th</sup> models, then the 10<sup>th</sup> and 20<sup>th</sup> models, and so on.
* `GAME_NUM` sets the number of games to play for each model pair, e.g., `100`.
* `OPTION` sets optional arguments, e.g., `-conf_str` sets additional configurations.

For detailed arguments, run `tools/quick-run.sh self-eval -h`.

Sample commands:
```bash
# evaluate a TicTacToe training session using "tictactoe_play.cfg", run 100 games for each model pair: 0<sup>th</sup> vs 10<sup>th</sup>, 10<sup>th</sup> vs 20<sup>th</sup>, ...
tools/quick-run.sh self-eval tictactoe tictactoe_az_1bx256_n50-cb69d4 tictactoe_play.cfg 10 100

# evaluate a TicTacToe training session using its training config, overwrite several settings for evaluation
tools/quick-run.sh self-eval tictactoe tictactoe_az_1bx256_n50-cb69d4 tictactoe_az_1bx256_n50-cb69d4/*.cfg 10 100 -conf_str actor_select_action_by_count=true:actor_use_dirichlet_noise=false:actor_num_simulation=200

# use more threads for faster evaluation
tools/quick-run.sh self-eval tictactoe tictactoe_az_1bx256_n50-cb69d4 tictactoe_play.cfg 10 100 --num_threads 20
```

Note that evaluation is unnecessary for Atari games.

The evaluation results are stored inside `FOLDER`, in a subfolder named `self_eval` by default, which contains the following records:
* `elo.csv` saves the evaluated model strength in Elo rating
* `elo.png` plots the Elo rating of `elo.csv`
* `5000_vs_0`, `10000_vs_5000`, and other folders keep game trajectory records for each evaluated model pair.

## Fight-Evaluation

Fight-evaluation evaluates the relative strengths between the same iterations of two training sessions, i.e., it compares the learning results of two network models.

```bash
tools/quick-run.sh fight-eval GAME_TYPE FOLDER1 FOLDER2 [CONF_FILE1] [CONF_FILE2] [INTERVAL] [GAMENUM] [OPTION]...
```

* `GAME_TYPE` sets the target game, e.g., `tictactoe`.
* `FOLDER1` and `FOLDER2` set the two folders to be evaluated.
* `CONF_FILE1` and `CONF_FILE2` set the config files for both folders; if `CONF_FILE2` is unspecified, `FOLDER2` will uses `CONF_FILE1` for evaluation.
* `INTERVAL` sets the iteration interval between each model pair to be evaluated, e.g. `10` indicates to match the i<sup>th</sup> models of both folders, then the i+10<sup>th</sup> models, and so on.
* `GAME_NUM` sets the number of games to play for each model pair, e.g., `100`.
* `OPTION` sets optional arguments, e.g., `-conf_str` sets additional configurations.

For detailed arguments, run `tools/quick-run.sh fight-eval -h`.

Sample commands:

```bash
# evaluate two training results using "tictactoe_play.cfg" for both programs, run 100 games for each model pair
tools/quick-run.sh fight-eval tictactoe tictactoe_az_1bx256_n50-cb69d4 tictactoe_az_1bx256_n50-731a0f tictactoe_play.cfg 10 100

# evaluate two training results using "tictactoe_cb69d4.cfg" and "tictactoe_731a0f.cfg" for the former and the latter, respectively
tools/quick-run.sh fight-eval tictactoe tictactoe_az_1bx256_n50-cb69d4 tictactoe_az_1bx256_n50-731a0f tictactoe_cb69d4.cfg tictactoe_731a0f.cfg 10 100
```

The evaluation results are stored inside `FOLDER1`, in a subfolder named `[FOLDER1]_vs_[FOLDER2]_eval` by default, in which records of each model pair are named by the step index of model files, e.g., `0`, `5000`, and so on.

## Miscellaneous Evaluation Tips

### Configurations for evaluation

Evaluation requires a different configuration from training, e.g., use more simulations and disable noise to always select the best action.
```
actor_num_simulation=400
actor_select_action_by_count=true
actor_select_action_by_softmax_count=false
actor_use_dirichlet_noise=false
actor_use_gumbel_noise=false
```

In addition, sometimes played games become too similar. 
To prevent this, use random rotation (for AlphaZero only) or even add softmax/noise back.
```
actor_use_random_rotation_features=true
actor_select_action_by_count=false
actor_select_action_by_softmax_count=true
```
