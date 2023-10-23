# Console

MiniZero supports the [Go Text Protocol (GTP)](http://www.lysator.liu.se/~gunnar/gtp/) and has a built-in console for easy communication with human operators or external programs.

```bash
tools/quick-run.sh console GAME_TYPE FOLDER|MODEL_FILE [CONF_FILE] [OPTION]...
```

* `GAME_TYPE` sets the target game, e.g., `tictactoe`.
* `FOLDER` or `MODEL_FILE` sets either the folder or the model file (`*.pt`).
* `CONF_FILE` sets the config file for console.
* `OPTION` sets optional arguments, e.g., `-conf_str` sets additional configurations.

For detailed arguments, run `tools/quick-run.sh console -h`.

Sample commands:

```bash
# run a console with the latest model inside "tictactoe_az_1bx256_n50-cb69d4" using config "tictactoe_play.cfg"
tools/quick-run.sh console tictactoe tictactoe_az_1bx256_n50-cb69d4 tictactoe_play.cfg

# run a console with a specified model file using config "tictactoe_play.cfg"
tools/quick-run.sh console tictactoe tictactoe_az_1bx256_n50-cb69d4/model/weight_iter_25000.pt tictactoe_play.cfg

# run a console with the latest model inside "tictactoe_az_1bx256_n50-cb69d4" using its default config file, and overwrite several settings for console
tools/quick-run.sh console tictactoe tictactoe_az_1bx256_n50-cb69d4 -conf_str actor_select_action_by_count=true:actor_use_dirichlet_noise=false:actor_num_simulation=200
```

Note that the console requires a trained network model.

After the console starts successfully, a message "Successfully started console mode" will be displayed.
Then, use [GTP commands](https://www.gnu.org/software/gnugo/gnugo_19.html) to interact with the program, e.g., `genmove`.

## Miscellaneous Console Tips

### Attach MiniZero to GoGui

[GoGui](https://github.com/Remi-Coulom/gogui) provides a graphical interface for board game AI programs, it provides two tools, `gogui-server` and `gogui-client`, to attach programs that support GTP console.

To attach MiniZero to GoGui, specify the `gogui-server` port via `-p`, which will automatically starts the MiniZero console with the `gogui-server`.

```bash
# host the console at port 40000 using gogui-server
tools/quick-run.sh console tictactoe tictactoe_az_1bx256_n50-cb69d4 tictactoe_play.cfg -p 40000
```
