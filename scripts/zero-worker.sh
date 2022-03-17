#!/bin/bash

if [ $# -lt 3 ]
then
	echo "Usage: ./zero-worker.sh host port [sp|op] [-g GPU_LIST] [-b BATCH_SIZE] [-c CPU_THREAD_PER_GPU]"
	exit 1
else
	HOST=$1
	PORT=$2
	TYPE=$3
	shift
	shift
	shift
	
	# default arguments
	NUM_GPU=$(nvidia-smi -L | wc -l)
	GPU_LIST=$(echo $NUM_GPU | awk '{for(i=0;i<$1;i++)printf i}')
	BATCH_SIZE=64
	MAX_NUM_CPU_THREAD_PER_GPU=4
fi

while :; do
	case $1 in
		-g|--gpu) shift; GPU_LIST=$1; NUM_GPU=${#GPU_LIST}
		;;
		-b|--batch_Size) shift; BATCH_SIZE=$1
		;;
		-c|--cpu_thread_per_gpu) shift; MAX_NUM_CPU_THREAD_PER_GPU=$1
		;;
		"") break
		;;
		*) echo "Unknown argument: $1"; exit 1
		;;
	esac
	shift
done

# arguments
MAX_NUM_CPU_THREAD=$((MAX_NUM_CPU_THREAD_PER_GPU*NUM_GPU))
NUM_CPU_THREAD=$(lscpu -p=CORE | grep -v "#" | wc -l)
if [ $NUM_CPU_THREAD -gt $MAX_NUM_CPU_THREAD ]; then
	NUM_CPU_THREAD=$MAX_NUM_CPU_THREAD
fi

# every command in checkCommands must be executable
checkCommands=(Release/minizero rm flock kill nvidia-smi)

for name in "${checkCommands[@]}"
do
	which $name > /dev/null
	if [ $? -ne 0 ]
	then
		echo "Cannot run $name, exit."
		exit 1
	fi
done

function closeFd()
{
	local fd=$1
	eval "exec {fd}<&-"
	eval "exec {fd}>&-"
}

function onExit()
{
	if [[ ! -z $broker_fd ]]
	then
		[[ -z  $selfPlay_pid ]] || flock -x $broker_fd kill -CONT $selfPlay_pid
		[[ -z  $selfPlay_pid ]] || flock -x $broker_fd kill $selfPlay_pid 2>/dev/null
		rm -f $broker_fd
		closeFd $broker_fd
	fi
	exit
}

# kill all background process when exit
trap onExit SIGINT SIGTERM EXIT
trap true SIGALRM

function kill_descendant_processes()
{
	local pid="$1"
	local and_self="${2:-false}"
	if children="$(pgrep -P "$pid")"; then
		for child in $children; do
			kill_descendant_processes "$child" true
		done
	fi
	if [[ "$and_self" == true ]]; then
		kill "$pid" 2>/dev/null
	fi
}

while true
do
	# try to connect to broker
	selfPlay_pid=""
	broker_fd=""
	exec {broker_fd}<>/dev/tcp/$HOST/$PORT
	if [[ -z $broker_fd ]]
	then
		# connect failed
		echo "connect to $HOST:$PORT failed, retry after 60s..."
		sleep 60
	else
		echo "connect success"
		while true
		do
			# read from broker
			read -u $broker_fd line
			error_code=$?
			if [ $error_code -eq 0 ]
			then
				if [ "$line" != "keep_alive" ]
				then
					echo "read: $line"
				fi
				
				if [ "$line" == "Info" ]
				then
					#NAME="hostname_GPU_LIST@ip"
					NAME=$(hostname)"_"$GPU_LIST
					
					# format: Info name gpu_list worker_type
					echo "Info $NAME $TYPE"
					echo "Info $NAME $TYPE" 1>&$broker_fd
				elif [ "$line" == "keep_alive" ]
				then
					true
					#echo "read: $line"
				elif [[ $line =~ ^Job_SelfPlay\ (.+) ]]
				then
					if [ "$TYPE" == "sp" ]
					then
						# kill previous selfPlay process
						[[ -z  $selfPlay_pid ]] || flock -x $broker_fd kill $selfPlay_pid 2>/dev/null

						# format: Self-play train_dir conf_str
						var=(${BASH_REMATCH[1]})
						CONF_FILE=$(ls ${var[0]}/*.cfg)
						CONF_STR="${var[1]}:actor_num_threads=${NUM_CPU_THREAD}:actor_num_parallel_games=$((${BATCH_SIZE}*${NUM_GPU}))"
						CUDA_DEVICES=$(echo ${GPU_LIST} | awk '{ split($0, chars, ""); printf(chars[1]); for(i=2; i<=length(chars); ++i) { printf(","chars[i]); } }')
						echo "CUDA_VISIBLE_DEVICES=${CUDA_DEVICES} Release/minizero -mode sp -conf_file ${CONF_FILE} -conf_str \"${CONF_STR}\""
						CUDA_VISIBLE_DEVICES=${CUDA_DEVICES} Release/minizero -conf_file ${CONF_FILE} -conf_str "${CONF_STR}" -mode sp 1>&$broker_fd &
						selfPlay_pid=$!
					elif [ "$TYPE" == "op" ]
					then
						echo "skip Job_Selfplay"
					fi
				elif [[ $line =~ ^Job_Optimization\ (.+) ]]
				then
					if [ "$TYPE" == "sp" ]
					then
						echo "skip Job_Optimization"
					elif [ "$TYPE" == "op" ]
					then
						var=(${BASH_REMATCH[1]})
						CONF_FILE=$(ls ${var[0]}/*.cfg)
						# py/Train.py train_dir model sgf_start sgf_end conf_file
						echo "PYTHONPATH=. python minizero/learner/train.py ${var[0]} ${var[1]} ${var[2]} ${var[3]} ${CONF_FILE} 2>>${var[0]}/op.log"
						PYTHONPATH=. python minizero/learner/train.py ${var[0]} ${var[1]} ${var[2]} ${var[3]} ${CONF_FILE} 1>&$broker_fd 2>>${var[0]}/op.log
					fi
				elif [ "$line" == "Job_Done" ]
				then
					[[ -z  $selfPlay_pid ]] || flock -x $broker_fd kill $selfPlay_pid 2>/dev/null
				else
					echo "read format error"
					echo "msg: $line"
					
					# close socket
					closeFd $broker_fd
					echo "disconnected from broker"
					sleep 10
					
					break
				fi
			elif [ $error_code -gt 128 ]
			then
				# read timeout, do nothing
				true
			else
				# disconnected
				echo "disconnected from broker"
				sleep 10
				
				break
			fi
		done
	fi

	# disconnected, clean up running process
	if [[ ! -z  $broker_fd ]]
	then
		[[ -z  $selfPlay_pid ]] || flock -x $broker_fd kill $selfPlay_pid 2>/dev/null
	fi
done
