# Evaluation

This document guides you in evaluating the strength of trained AI models.

## Use Quick-Run Script

Specify the target game, the folder containing trained models, and other options to start an evaluation session.

```bash
tools/quick-run.sh self-eval GAME_TYPE FOLDER [CONF_FILE] [INTERVAL] [GAMENUM] [OPTION]...
```

* `GAME_TYPE` sets the target game, e.g., `tictactoe`.
* `FOLDER` sets the folder containing trained models.
* `CONF_FILE` sets the configuration file to use.
* `INTERVAL` sets the iteration interval for evaluation.
* `GAMENUM` sets the number of games for each evaluation.

For comparing two different models:
```bash
tools/quick-run.sh fight-eval GAME_TYPE FOLDER1 FOLDER2 [CONF_FILE1] [CONF_FILE2] [INTERVAL] [GAMENUM] [OPTION]...
```

For more details, run `tools/quick-run.sh self-eval -h` or `tools/quick-run.sh fight-eval -h`.
