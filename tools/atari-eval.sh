#!/bin/bash
set -e

usage()
{
	echo "Usage: ./atari-eval.sh [Atari game name] [Folder] [Config] [Interval] [Game num] [OPTION...]"
	echo ""
	echo "  -h        , --help                 Give this help list"
	echo "  -s        ,                        Start evaluation from which model in the folder, e.g. 0 means start from the first model"
	echo "  -d        ,                        Assign evaluation result folder name"
	echo "  -g        , --gpu                  Assign available GPUs for worker, e.g. 0123"
	echo "  -b, --batch_size           Assign the batch size in self-play worker (default = 32)"
	echo "  -c, --cpu_thread_per_gpu   Assign the number of CPUs for each GPU in self-play worker (default = 4)"
	echo "            , --sp_executable_file   Assign the path for self-play executable file"
	echo "  -conf_str ,                        Overwrite configuration file"
	exit 1
}

# check argument
if [[ $# -lt 5 ]] || [[ $(($# % 2)) -eq 0 ]]; then
	usage
else
	GAME_NAME=$1; shift
	FOLDER=$1; shift
	CONF_FILE=$1; shift
	INTERVAL=$1; shift
	GAMENUM=$1; shift

	# default arguments
	START=0
	NAME="eval"
	num_gpu=$(nvidia-smi -L | wc -l)
	gpu_list=$(echo $num_gpu | awk '{for(i=0;i<$1;i++)printf i}')
	batch_size=32
	max_num_cpu_thread_per_gpu=4
	additional_conf_str=""
	
fi
game_type="atari"
sp_executable_file=build/${game_type}/minizero_${game_type}
while :; do
	case $1 in
		-h|--help) shift; usage
		;;
		-s) shift; START=$1
		;;
        -d) shift; NAME=$1
		;;
		-g|--gpu) shift; gpu_list=$1; num_gpu=${#gpu_list}
		;;
		-b|--batch_Size) shift; batch_size=$1
		;;
		-c|--cpu_thread_per_gpu) shift; max_num_cpu_thread_per_gpu=$1
		;;
		--sp_executable_file) shift; sp_executable_file=$1
		;;
		-conf_str) shift; additional_conf_str=":$1"
		;;
		"") break
		;;
		*) echo "Unknown argument: $1"; usage
		;;
	esac
	shift
done

# arguments
cuda_devices=$(echo ${gpu_list} | awk '{ split($0, chars, ""); printf(chars[1]); for(i=2; i<=length(chars); ++i) { printf(","chars[i]); } }')
max_num_cpu_thread=$((max_num_cpu_thread_per_gpu*num_gpu))
num_cpu_thread=$(lscpu -p=CORE | grep -v "#" | wc -l)
if [[ $num_cpu_thread -gt $max_num_cpu_thread ]]; then
	num_cpu_thread=$max_num_cpu_thread
fi

if [[ $batch_size -gt $(((${GAMENUM}+${num_gpu}-1)/${num_gpu})) ]]; then
	batch_size=$(((${GAMENUM}+${num_gpu}-1)/${num_gpu}))
fi

function closeFd()
{
	local fd=$1
	eval "exec {fd}<&-"
	eval "exec {fd}>&-"
}
function onInt()
{
	set +e
	kill -9 "$pid"
	exit
}
function onExit()
{
	wait "$pid"
	if [[ ! -z $fd0 ]]
	then
		closeFd $fd0
	fi
    if [[ ! -z $fd1 ]]
	then
		closeFd $fd1
	fi
	if [[ ! -z $lockfile ]]
	then
		rm -f $lockfile
	fi
	rm -f $tmp_file $fifo0 $fifo1
	exit
}

EVAL_FOLDER="${FOLDER}/$NAME"

if [[ ! -d "${FOLDER}" ]]; then
    echo "${FOLDER} not exists!"
    exit 1
fi

if [[ ! -d $EVAL_FOLDER ]]; then
    mkdir -p $EVAL_FOLDER
fi

# get fifos
tmp_file=$(mktemp)
fifo0="$tmp_file-0"
fifo1="$tmp_file-1"
mkfifo $fifo0
mkfifo $fifo1
fd0=""
fd1=""
exec {fd0}<>"$fifo0"
exec {fd1}<>"$fifo1"

# kill all background process when exit
trap onExit SIGTERM EXIT
trap onInt SIGINT
trap true SIGALRM

first_model_file=$(ls $FOLDER/model | grep ".pt$" | sort -V | head -n 1)
conf_str="zero_num_games_per_iteration=$GAMENUM:zero_actor_ignored_command=:zero_actor_intermediate_sequence_length=0:env_atari_name=${GAME_NAME}:program_quiet=true:zero_training_directory=$FOLDER:nn_file_name=$FOLDER/model/$first_model_file:actor_num_threads=${num_cpu_thread}:actor_num_parallel_games=$((${batch_size}*${num_gpu}))${additional_conf_str}"
echo ${sp_executable_file} -conf_file $CONF_FILE -conf_str ${conf_str} -mode sp
CUDA_VISIBLE_DEVICES=${cuda_devices} ${sp_executable_file} -conf_file $CONF_FILE -conf_str ${conf_str} -mode sp 0<&$fd0 1>&$fd1 &
pid=$!
models=($(ls $FOLDER/model | grep ".pt$" | sort -V))
for((i=$START;i<${#models[@]};i=i+$INTERVAL))
do
	model_file=${models[$i]}
	CNT=1
	sgffile=$EVAL_FOLDER/${model_file:12:-3}.sgf
	lockfile=$EVAL_FOLDER/${model_file:12:-3}.lock
	played_game_num=$(2>/dev/null cat $sgffile | wc -l)
	if [[ $played_game_num -ge $GAMENUM ]]; then
		continue
	fi
	# Check is Lock File exists, if not create it
	if { set -C; 2>/dev/null >$lockfile; }; then
		echo "load_model $FOLDER/model/$model_file">&$fd0
		echo "reset_actors">&$fd0
		echo "start">&$fd0
		while [ $played_game_num -lt $GAMENUM ]
		do	
			# read one selfplay game
			read -u $fd1 line
			game_model_file=$(echo $line | grep -oE "EV\[[^]]*\]" | sed 's/EV\[//g;s/\]//g')
			# discard previous self-play games
			if [[ $game_model_file != $model_file ]]; then
				continue
			fi
			echo $line | sed -r 's/^[^(]*//g' | sed -r 's/OBS\[([^]]*)\]//g'  | sed -r 's/\|[PVR]:[^]]*/\|/g' | sed -r 's/ #//g' >> "$sgffile"
			played_game_num=$(($played_game_num+1))
		done
		echo "stop">&$fd0
		rm -f $lockfile
	else
		continue
	fi
done
lockfile=""
echo "quit">&$fd0
