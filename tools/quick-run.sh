#!/bin/bash

usage() {
    case "$1" in
    train)
        echo "Usage: $0 train GAME_TYPE [CONF_FILE|ALGORITHM] END_ITER [OPTION]..."
        echo "Launch a zero training session (incl. zero-server and zero-workers) to train a model for specific game."
        echo ""
        echo "Required arguments:"
        echo "  GAME_TYPE: $(find ./ ../ -maxdepth 2 -name build.sh -exec grep -m1 support_games {} \; -quit | sed -E 's/.+\("|"\).*//g;s/" "/, /g')"
        echo "  CONF_FILE|ALGORITHM: set the configure file (*.cfg) to use, or one of supported zero algorithms: az, mz, gaz, gmz"
        echo "  END_ITER: the total number of iterations for training"
        echo ""
        echo "Optional arguments:"
        echo "  -h,        --help                 Show this help message and exit"
        echo "  -g,        --gpu                  Assign available GPUs for workers, e.g. 0123"
        echo "  -n,        --name                 Assign name for training directory"
        echo "  -np,       --name_prefix          Add prefix name for default training directory name"
        echo "  -ns,       --name_suffix          Add suffix name for default training directory name"
        echo "  -conf_file                        Specify the configure file to use"
        echo "  -conf_str                         Overwrite settings in the configure file"
        echo "  -gen                              Generate a configure file for current settings"
        echo "  -b,        --batch_size           Assign the batch size for self-play workers (default 64)"
        echo "  -c,        --cpu_thread_per_gpu   Assign the number of CPUs for each GPU for self-play workers (default 4)"
        echo "             --sp_progress          Show the self-play progress (default hidden)"
        echo "             --sp_conf_str          Set additional settings for self-play workers"
        ;;
    self-eval)
        echo "Usage: $0 self-eval GAME_TYPE FOLDER [CONF_FILE] [INTERVAL] [GAMENUM] [OPTION]..."
        find ./ ../ -maxdepth 2 -name self-eval.sh -exec {} -h \; -quit | tail -n+2 | grep -Ev "^ +-[hb][, ]|sp_executable_file"
        ;;
    fight-eval)
        echo "Usage: $0 fight-eval GAME_TYPE FOLDER1 FOLDER2 [CONF_FILE1] [CONF_FILE2] [INTERVAL] [GAMENUM] [OPTION]..."
        find ./ ../ -maxdepth 2 -name fight-eval.sh -exec {} -h \; -quit | tail -n+2 | grep -Ev "^ +-[hb][, ]|sp_executable_file"
        ;;
    console)
        echo "Usage: $0 console GAME_TYPE FOLDER|MODEL [CONF_FILE] [OPTION]..."
        echo "Launch the GTP shell console to interact with a trained model."
        echo ""
        echo "Required arguments:"
        echo "  GAME_TYPE: $(find ./ ../ -maxdepth 2 -name build.sh -exec grep -m1 support_games {} \; -quit | sed -E 's/.+\("|"\).*//g;s/" "/, /g')"
        echo "  FOLDER|MODEL: set the model folder, OR a model file (*.pt)"
        echo "  CONF_FILE: set the configure file (*.cfg) to use"
        echo ""
        echo "Optional arguments:"
        echo "  -h,        --help   Show this help message and exit"
        echo "  -g,        --gpu    Assign available GPUs for the program, e.g. 0123"
        echo "  -p,        --port   Launch the console using gogui-server at a specified port"
        echo "  -conf_file          Specify the configure file to use"
        echo "  -conf_str           Overwrite settings in the configure file"
        ;;
    *)
        echo "Usage: $0 MODE [OPTION]..."
        echo "Launch the pre-defined minizero procedure."
        echo ""
        echo "Required arguments:"
        echo "  MODE: train, self-eval, fight-eval, console"
        echo ""
        echo "Use $0 MODE -h for usage of each MODE."
        ;;
    esac
}

if [[ ! $container ]] && [[ ! $SCRIPT ]]; then # workaround for the issue caused by exec -it
    export SCRIPT="$0 $@"
    script -q -c "$SCRIPT" /dev/null
    exit $?
fi

colorize() { cat; }
log() { echo "${@:2}" | colorize $1 >&2; }

[[ $@ =~ --color\ ?(auto|always|never)? ]] && color=${BASH_REMATCH[1]:-auto}
if [[ $color == always ]] || ([[ $color != never ]] && [ -t 1 ] && [ -t 2 ]); then # enable color support
    export TERM=xterm-256color
    colorize() {
        local fmtset=$(tput sgr0)
        local fmtclr=${fmtset}
        case "${1:-N}" in
            r)  fmtset=$(tput setaf 1); ;; # red
            g)  fmtset=$(tput setaf 2); ;; # green
            y|WARN*|OUT_TRAIN_SP)  fmtset=$(tput setaf 3); ;; # yellow
            b)  fmtset=$(tput setaf 4); ;; # blue
            m)  fmtset=$(tput setaf 5); ;; # magenta
            c)  fmtset=$(tput setaf 6); ;; # cyan
            w)  fmtset=$(tput setaf 7); ;; # white
            K|OUT_CONSOLE_ERR)  fmtset=$(tput setaf 8); ;; # gray
            R|ERR*)  fmtset=$(tput setaf 9); ;; # light red
            G)  fmtset=$(tput setaf 10); ;; # light green
            Y)  fmtset=$(tput setaf 11); ;; # light yellow
            B)  fmtset=$(tput setaf 12); ;; # light blue
            M)  fmtset=$(tput setaf 13); ;; # light magenta
            C|OUT_TRAIN_OP)  fmtset=$(tput setaf 14); ;; # light cyan
            W|OUT_BUILD|OUT_TRAIN|OUT_EVAL|OUT_CONSOLE_GOGUI)  fmtset=$(tput setaf 15); ;; # light white
            k)  fmtset=$(tput setaf 16); ;; # black
            n)  fmtset=$(tput sgr0); ;; # none
            N|INFO)  unset fmtset fmtclr; ;; # none
        esac
        local code
        while { IFS= read -r -t ${TMOUT:-0.1} line; (( (code=$?) != 1 )); }; do
            case $code in
                0) echo    "${fmtset}${line}${fmtclr}"; ;;
                *) echo -n "${fmtset}${line}${fmtclr}"; ;;
            esac
        done
    }
fi

# ================================ PARSING OPTIONS ================================

# parse required arguments
mode=${1,,}; shift
game=${1,,}; shift
if [[ $game =~ ^(|-h|--help)$ ]]; then # help format "train -h"
    usage $mode
    exit 0
fi
case "$mode" in
train) # [CONF_FILE|ALGORITHM] END_ITER
    mode=train
    [[ $1 == *.cfg ]] && { conf_file=$1; shift; }
    [[ ${1,,} =~ ^(az|mz|gaz|gmz)$ ]] && { train_algorithm=$BASH_REMATCH; shift; }
    [[ $1 =~ ^[0-9]+$ ]] && { zero_end_iteration=$1; shift; }
    ;;
self-eval) # FOLDER [CONF_FILE] [INTERVAL] [GAME_NUM]
    mode=self-eval
    [[ $1 != -* ]] && [ -d "$1" ] && { eval_dir=$1; shift; }
    [[ $1 == *.cfg ]] && { conf_file=$1; shift; }
    [[ $1 =~ ^[0-9]+$ ]] && { eval_interval=$1; shift; }
    [[ $1 =~ ^[0-9]+$ ]] && { eval_game_num=$1; shift; }
    ;;
fight-eval) # FOLDER1 FOLDER2 [CONF_FILE1] [CONF_FILE2] [INTERVAL] [GAME_NUM]
    mode=fight-eval
    [[ $1 != -* ]] && [ -d "$1" ] && { eval_dir=$1; shift; }
    [[ $1 != -* ]] && [ -d "$1" ] && { eval_dir_2=$1; shift; }
    [[ $1 == *.cfg ]] && { conf_file=$1; shift; }
    [[ $1 == *.cfg ]] && { conf_file_2=$1; shift; }
    [[ $1 =~ ^[0-9]+$ ]] && { eval_interval=$1; shift; }
    [[ $1 =~ ^[0-9]+$ ]] && { eval_game_num=$1; shift; }
    ;;
console) # FOLDER|MODEL [CONF_FILE]
    mode=console
    [[ $1 != -* ]] && { eval_dir=$1; shift; }
    [[ $1 == *.cfg ]] && { conf_file=$1; shift; }
    ;;
-h|--help|"")
    usage $game # help format "-h train"
    exit 0
    ;;
*)
    log ERR "Unsupported mode: $mode"
    exit 1
    ;;
esac
eval $(grep -m1 support_games scripts/build.sh)
if ! printf "%s\n" "${support_games[@]}" | grep -q "^$game$"; then
    if [[ $1 == -h || $1 == --help ]]; then
        usage $game # assume usage for MODE
        exit 0
    fi
    atari_games=(
        alien amidar assault asterix asteroids atlantis bank_heist battle_zone beam_rider berzerk
        bowling boxing breakout centipede chopper_command crazy_climber defender demon_attack double_dunk enduro
        fishing_derby freeway frostbite gopher gravitar hero ice_hockey jamesbond kangaroo krull
        kung_fu_master montezuma_revenge ms_pacman name_this_game phoenix pitfall pong private_eye qbert riverraid
        road_runner robotank seaquest skiing solaris space_invaders star_gunner surround tennis time_pilot
        tutankham up_n_down venture video_pinball wizard_of_wor yars_revenge zaxxon
    )
    if [[ $game =~ ^($( IFS=\|; echo "${atari_games[*]}"; ))$ ]]; then
        env_atari_name=$game
        conf_str=env_atari_name=${env_atari_name}${conf_str:+:}${conf_str}
        game=atari
    else
        log ERR "Unsupported game: $game"
        exit 1
    fi
fi

# parse optional arguments
while [[ $1 ]]; do
    case "$1" in
    -conf_file)         conf_file=$2; shift; ;;
    -conf_str)          conf_str+=${conf_str:+:}${2}; shift; ;;
    -g|--gpu)           CUDA_VISIBLE_DEVICES=$(echo ${2//,/} | grep -o . | xargs | tr ' ' ','); shift; ;;
    -n|--name)          train_dir=$2; shift; ;;
    -np|--name_prefix)  name_prefix=$2; shift; ;;
    -ns|--name_suffix)  name_suffix=$2; shift; ;;
    -b|--batch_size)    batch_size=$2; shift; ;;
    -c|--cpu_thread_per_gpu|--num_threads)  num_threads=$2; shift; ;;
    --color)            color=$2; shift; ;;
    --sp_progress)      [[ $2 =~ true|false ]] && { sp_progress=$2; shift; } || { sp_progress=true; } ;;
    --sp_conf_str)      sp_conf_str=$2; shift; ;;
    -gen)               gen_conf_file=$2; shift; ;;
    -s)                 eval_start_index=$2; shift; ;;
    -d)                 eval_folder_name=$2; shift; ;;
    -p|--port)          port=$2; zero_server_port=$2; shift; ;;
    -h)                 usage $mode; exit 0; ;;
    *)                  log ERR "Unsupported argument: $1"; exit 1; ;;
    esac
    shift
done

# ================================ STEUP ================================

git_info=$(git describe --abbrev=6 --dirty --always --exclude '*' 2>/dev/null || echo xxxxxx)
session_name=minizero_$game.$git_info.$mode.$(date '+%Y%m%d%H%M%S')
log INFO "MiniZero quick-run session: $session_name"

if [[ $conf_file ]] && [ ! -e "$conf_file" ]; then
    log ERR "Failed to load config file: $conf_file"
    exit 1
fi
if [[ $conf_str ]] && ! eval "$(tr ':' '\n' <<< $conf_str)"; then
    log ERR "Incorrect config string syntax: $conf_str"
fi
export CUDA_VISIBLE_DEVICES=${CUDA_VISIBLE_DEVICES:-$(nvidia-smi --query-gpu=index --format=csv,noheader | xargs | tr ' ' ',')}

# check required commands and drivers
for cmd in nvidia-smi; do
    if ! which $cmd >/dev/null; then
        log ERR "Command unavailable: $cmd"
        exit 1
    fi
done

# check container
container_tool=$(basename $(which podman || which docker) 2>/dev/null)
if [[ $container_tool ]] && [[ ! $container ]]; then
    log INFO "Start a container for launching the program ..."
    container=$(scripts/start-container.sh --name $session_name -d | tail -n1)
    if [[ ! $container == $($container_tool ps -n 1 -q)* ]]; then
        unset container
        log ERR "Failed to start container"
        exit 1
    fi
    log INFO "Container: $container"
    launch="$container_tool exec -e CUDA_VISIBLE_DEVICES=$CUDA_VISIBLE_DEVICES -it $container"

    cleanup() {
        rm .$session_name.* 2>/dev/null

        kill $(jobs -p) ${!PID[@]} 2>/dev/null
        if ! $container_tool kill $container >/dev/null; then
            log WARN "Failed to terminate the container: $container"
        # else
            # log INFO "Container terminated: $container"
        fi
    }
else
    if [[ $container == podman || $container == docker ]]; then
        log INFO "Already running inside the container, will launch the program directly"
    else
        log WARN "Container tool unavailable, will launch the program directly"
    fi
    unset launch

    cleanup() {
        rm .$session_name.* 2>/dev/null

        declare -A PPID_
        while read -r pid ppid; do
            PPID_[$ppid]+="$pid "
        done <<< $(ps -o "%p %P" --no-headers)

        local pids=(${PPID_[$$]})
        while [[ $pids ]]; do
            echo $pids
            pids=(${pids[@]:1} ${PPID_[$pids]})
        done | xargs kill 2>/dev/null
    }
fi
trap 'exit 127' INT TERM
trap 'code=$?; cleanup >/dev/null; exit $code' EXIT
trap - SIGUSR1

log INFO "Building the executable for $game ..."
$launch scripts/build.sh $game | colorize OUT_BUILD >&2
if (( ${PIPESTATUS[0]} )); then
    log ERR "Failed to build the executable for $game"
    exit 1
fi
project=$(grep -Fq '"${PROJECT_NAME}_${GAME_TYPE}" EXE_FILE_NAME' CMakeLists.txt && grep -E "project\(.+\)" CMakeLists.txt | sed "s/project(//;s/)//")
if [[ $project ]]; then
    executable=build/${game}/${project}_${game}
else
    executable=$(ls -t build/${game}/*_${game} | head -n1)
    log WARN "Unable to retrieve executable name, will use auto-probed executable"
fi
if [ ! -e "$executable" ]; then
    log ERR "Failed to build the executable for $game (no such file)"
    exit 1
else
    log INFO "Executable: $executable"
fi

watchdog() {
    while IFS= read -r line; do
        [[ $line =~ $1 ]] && kill -SIGUSR1 $$
    done
}

if [[ $mode == train ]]; then # ================================ TRAIN ================================

    if [[ ! $conf_file ]]; then
        conf_file=$game-$git_info.cfg
        log WARN "Config file unspecified, generating default config file ..."
        [[ $conf_file == *dirty* ]] && [ -e $conf_file ] && rm $conf_file
        [ -e $conf_file ] || $launch $executable -gen $conf_file >/dev/null 2>&1
        if (( $? )) || [ ! -e $conf_file ]; then
            log ERR "Failed to generate config file: $conf_file"
            exit 1
        else
            log INFO "Default config file: $conf_file"
        fi
        auto_conf_file=true
    fi
    if [ ! -e "$conf_file" ]; then
        log ERR "Failed to load config file: $conf_file"
        exit 1
    fi

    if [[ $train_algorithm ]]; then
        case "$train_algorithm" in
        g*)  alg_conf_str+=${alg_conf_str:+:}actor_num_simulation=16:actor_use_dirichlet_noise=false:actor_use_gumbel=true:actor_use_gumbel_noise=true; ;;& # resume!
        *az) alg_conf_str+=${alg_conf_str:+:}nn_type_name=alphazero; ;;
        *mz) alg_conf_str+=${alg_conf_str:+:}nn_type_name=muzero; ;;
        esac
        if [[ $train_algorithm == *az && $game == atari ]]; then
            log ERR "Unsupported training algorithm: $train_algorithm"
            exit 1
        fi
        conf_str=${alg_conf_str}${conf_str:+:}${conf_str}
        log INFO "Use training algorithm $train_algorithm, set additional config: $alg_conf_str"
    elif [[ ! $auto_conf_file ]]; then
        : # Config file provided
        env_atari_name=$({ grep env_atari_name= $conf_file || echo =; } | cut -d= -f2)
    else
        log WARN "Neither config file nor training algorithm is specified"
    fi

    if [[ $game == atari ]] && [[ ! $env_atari_name ]]; then
        log WARN "Config env_atari_name unspecified, will use default game: $({ grep env_atari_name= $conf_file || echo =unknown; } | cut -d= -f2)"
    fi

    if [[ ! $zero_server_port ]]; then
        zero_server_port=$({ grep zero_server_port= $conf_file || echo =$((58000+RANDOM%2000)); } | cut -d= -f2)
    fi
    if [[ ! $conf_str == *zero_server_port=* ]]; then
        conf_str+=${conf_str:+:}zero_server_port=$zero_server_port
    fi
    
    if [[ ! $zero_end_iteration ]]; then
        zero_end_iteration=$({ grep zero_end_iteration= $conf_file || echo =100; } | cut -d= -f2)
    fi

    if [[ $gen_conf_file ]]; then
        prev_time=$(date +%s)
        $launch $executable -gen ${gen_conf_file} -conf_file ${conf_file} \
            -conf_str "${conf_str:+$conf_str:}zero_end_iteration=$zero_end_iteration" | colorize OUT_BUILD
        save_time=$(date -r $gen_conf_file +%s 2>/dev/null)
        [[ $auto_conf_file == true ]] && rm $conf_file
        if (( save_time >= prev_time )); then
            log G "Config file has been saved: $gen_conf_file"
        elif [ ! -f $gen_conf_file ]; then
            log ERR "Failed to save config file: $gen_conf_file"
            exit 1
        fi
        exit 0
    fi

    if [[ ! $train_dir ]]; then
        train_dir=$($launch $executable -mode zero_training_name -conf_file ${conf_file} -conf_str "${conf_str}:program_quiet=true" 2>/dev/null | \
            tr -d '\r' | grep "^$game" | tail -n1)
        if [[ ! $train_dir ]]; then
            $launch $executable -mode zero_training_name -conf_file ${conf_file} -conf_str "${conf_str}" 2>&1 | sed "/^Failed to/d" | colorize ERR
            log ERR "Failed to generate default training folder name"
            exit 1
        fi
        train_dir=${name_prefix}${train_dir}${name_suffix}

    elif [[ $name_prefix || $name_suffix ]]; then
        log ERR "Model folder name and prefix/suffix should not be specified at the same time"
        exit 1
    fi

    log INFO "Training folder: $train_dir"

    declare -A PID

    # launch zero server
    {
        $launch scripts/zero-server.sh $game $conf_file $zero_end_iteration -n "$train_dir" -conf_str "$conf_str"
    } 2>&1 | tee >(watchdog "Worker Disconnection|^Failed to|Segmentation fault|Killed|Aborted|RuntimeError") | colorize OUT_TRAIN &
    PID[$!]=server
    server_pid=$!

    # wait until the initialization done
    tick=$(date +%s%3N)
    sleep 1
    while ps -p $server_pid >/dev/null && (( $( date=$(tail $train_dir/Training.log 2>/dev/null | grep -Eo "[0-9/]+_[0-9:.]+" | tail -n1);
            [[ $date ]] && echo $(date -d "${date/_/ }" +%s)${date:20} || echo 0; ) < tick )); do
        sleep 1
    done
    if ! ps -p $server_pid >/dev/null; then
        if [ -e $train_dir/sgf/$zero_end_iteration.sgf ] && (( $(ls -t $train_dir/model/*.pt | wc -l) > $zero_end_iteration )); then
            log WARN "Training already completed: $zero_end_iteration iterations"
            log INFO "Latest model: $(ls -t $train_dir/model/*.pt | head -n1)"
        else
            log ERR "Failed to start zero training server"
        fi
        exit 1
    fi

    # launch zero sp workers (one per available GPU)
    for GPU in ${CUDA_VISIBLE_DEVICES//,/ }; do
        {
            [[ $sp_progress == true ]] && sp_program_quiet=false || sp_program_quiet=true
            [[ $sp_conf_str != *program_quiet* ]] && sp_conf_str+=${sp_conf_str:+:}program_quiet=$sp_program_quiet
            $launch scripts/zero-worker.sh $game $(hostname) $zero_server_port sp -b ${batch_size:-64} -c ${num_threads:-4} -g $GPU ${sp_conf_str:+-conf_str "$sp_conf_str"}
        } 2>&1 | tee >(watchdog "^Failed to|Segmentation fault|Killed|Aborted|RuntimeError") | colorize OUT_TRAIN_SP &
        PID[$!]=sp$GPU
        unset sp_progress # only show the progress of the first sp
        sleep 1
    done

    # launch zero op worker
    {
        $launch scripts/zero-worker.sh $game $(hostname) $zero_server_port op -g ${CUDA_VISIBLE_DEVICES//,/}
    } 2>&1 | tee >(watchdog "^Failed to|Segmentation fault|Killed|Aborted|RuntimeError") | colorize OUT_TRAIN_OP &
    PID[$!]=op

    trap 'eval "log() { echo -en \"\r\" >&2; $(type log | tail -n+3); echo -en \"\r\" >&2; }"' SIGUSR1 # workaround for exec -it issue

    wait -n
    # check whether training successfully completed
    last_iter=$(grep -Eio "\[Iteration\] =+[0-9]+=+" $train_dir/Training.log 2>/dev/null | tail -n1)
    if [[ $(<<< $last_iter tr -d "A-z= ") == $zero_end_iteration ]] && \
        grep -F "$last_iter" -A 100 $train_dir/Training.log 2>/dev/null | grep -Eq "\[Optimization\] Finished."; then
        log g "Training successfully completed!"
        log INFO "Latest model: $(ls -t $train_dir/model/*.pt | head -n1)"
        code=0
    else
        log ERR "Training failed to complete"
        code=1
    fi

    exit $code

elif [[ $mode == self-eval ]]; then # ================================ SELF-EVAL ================================

    if [ ! -d "$eval_dir" ]; then
        log ERR "Evaluation folder unspecified"
        exit 1
    fi
    if [[ ! $conf_file ]]; then
        conf_file=$(ls -t "$eval_dir"/*.cfg 2>/dev/null | head -n1)
        if [ -e "$conf_file" ]; then
            log WARN "Use auto-probed config file: $conf_file"
        else
            log ERR "Failed to probe config file"
            exit 1
        fi
    fi
    eval_interval=${eval_interval:=10}
    eval_game_num=${eval_game_num:=100}

    opts=()
    opts+=(-g ${CUDA_VISIBLE_DEVICES//,/})
    opts+=(-s ${eval_start_index:=0})
    opts+=(-d ${eval_folder_name:=self_eval})
    opts+=(--num_threads ${num_threads:=2})

    $launch tools/self-eval.sh $game $eval_dir $conf_file $eval_interval $eval_game_num ${opts[@]} 2>&1 | colorize OUT_EVAL
    code=${PIPESTATUS[0]}
    if [[ $code == 0 ]] && [ -d "$eval_dir/$eval_folder_name" ]; then
        log INFO "Evaluation results: $eval_dir/$eval_folder_name"
    else
        log ERR "Evaluation failed to complete"
        code=1
    fi

    exit $code

elif [[ $mode == fight-eval ]]; then # ================================ FIGHT-EVAL ================================

    if [[ $game == atari || $game == puzzle2048 ]]; then
        log "Unsupported mode: $mode"
        exit 1
    fi
    if [ ! -d "$eval_dir" ] || [ ! -d "$eval_dir_2" ]; then
        log ERR "Evaluation folder unspecified"
        exit 1
    fi
    if [[ ! $conf_file ]]; then
        conf_file=$(ls -t "$eval_dir"/*.cfg 2>/dev/null | head -n1)
        if [ -e "$conf_file" ]; then
            log WARN "Use auto-probed config file: $conf_file"
        else
            log ERR "Failed to probe config file"
            exit 1
        fi
    fi
    [[ ! $conf_file_2 ]] && conf_file_2=$conf_file
    eval_interval=${eval_interval:=10}
    eval_game_num=${eval_game_num:=100}

    opts=()
    opts+=(-g ${CUDA_VISIBLE_DEVICES//,/})
    opts+=(-s ${eval_start_index:=0})
    opts+=(-d ${eval_folder_name:=$(basename ${eval_dir})_vs_$(basename ${eval_dir_2})_eval})
    opts+=(--num_threads ${num_threads:=2})

    $launch tools/fight-eval.sh $game $eval_dir $eval_dir_2 $conf_file $conf_file_2 $eval_interval $eval_game_num ${opts[@]} 2>&1 | colorize OUT_EVAL
    code=${PIPESTATUS[0]}
    if [[ $code == 0 ]] && [ -d "$eval_dir/$eval_folder_name" ]; then
        log INFO "Evaluation results: $eval_dir/$eval_folder_name"
    else
        log ERR "Evaluation failed to complete"
        code=1
    fi

    exit $code

elif [[ $mode == console ]]; then # ================================ CONSOLE ================================

    if [[ ! $nn_file_name ]]; then
        if [[ ! $eval_dir ]]; then
            [ -e "$conf_file" ] && env_board_size=$({ grep env_board_size= $conf_file || echo =0; } | cut -d= -f2)
            eval_dir=$(ls -dt ${game}_${env_board_size}x${env_board_size}_*/model ${game}_*/model 2>/dev/null | head -n1)
            nn_autoprob=true
        fi

        if [ -d "$eval_dir" ]; then
            nn_file_name=$(ls -t "$eval_dir"/model/*.pt "$eval_dir"/*.pt 2>/dev/null | head -n1)
        elif [[ $eval_dir == *.pt ]]; then
            nn_file_name=$eval_dir
        fi
    fi
    if [[ ! $nn_file_name ]]; then
        log ERR "Failed to probe model file"
        exit 1
    elif [ ! -e "$nn_file_name" ]; then
        log ERR "$nn_file_name unavailable"
        exit 1
    fi
    if [[ $nn_autoprob ]]; then
        log WARN "Use auto-probed model file: $nn_file_name"
    fi
    if [[ $nn_file_name =~ ^(.+)/model/.+.pt$ ]]; then
        eval_dir=${BASH_REMATCH[1]}
    else
        unset eval_dir
    fi

    if [[ ! $conf_file ]]; then
        conf_file=$(ls -t "$eval_dir"/*.cfg 2>/dev/null | head -n1)
        if [[ $conf_file ]]; then
            log WARN "Config file unspecified, will use existing config file in the training folder"
        else
            log ERR "Config file unavailable"
            exit 1
        fi
        auto_conf_file=true
    fi
    if [ ! -e "$conf_file" ]; then
        log ERR "Failed to load config file: $conf_file"
        exit 1
    fi

    if [[ ! $conf_str == *nn_file_name=* ]]; then
        conf_str+=${conf_str:+:}nn_file_name=$nn_file_name
    fi

    if [[ ! $port ]]; then # launch minizero console directly
        if [[ $launch ]]; then
            log ERR "Using indirect container mode, where stderr is also output to stdout!"
            log WARN "To avoid this, start a container then run $0 inside it"
        fi

        {
            $launch $executable -mode console -conf_file ${conf_file} -conf_str "${conf_str}"
        } 2> >(colorize OUT_CONSOLE_ERR >&2)

        code=$?

    else # launch minizero console using gogui-server
        if [[ ! $launch ]] && ! which gogui-server >/dev/null; then
            log ERR "Command unavailable: gogui-server"
            exit 1
        fi

        log INFO "Launch console with gogui-server at tcp://0.0.0.0:$port" # displaying this will trigger VS code to automatically start port forwarding
        {
            $launch gogui-server -port $port -loop -verbose "$executable -mode console -conf_file ${conf_file} -conf_str ${conf_str}"
        } 2>&1 | tee >(watchdog "^Address already in use|^Failed to|Segmentation fault|Killed|Aborted|RuntimeError") | colorize OUT_CONSOLE_GOGUI &
        PID[$!]=console

        trap 'log ERR "Console failed to start correctly"' SIGUSR1

        wait -n
        code=$?
    fi

    exit $code

fi
