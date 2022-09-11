#!/bin/bash

usage()
{
	echo "Usage: ./zero-worker.sh host port [sp|op] [OPTION...]"
	echo ""
	echo "  -h, --help                 Give this help list"
	echo "  -g, --gpu                  Assign available GPUs for worker, e.g. 0123"
	echo "  -b, --batch_size           Assign the batch size in self-play worker (default = 64)"
	echo "  -c, --cpu_thread_per_gpu   Assign the number of CPUs for each GPU in self-play worker (default = 4)"
	echo "    , --conf_str             Add additional configure string in self-play worker"
	echo "    , --sp_executable_file   Assign the path for self-play executable file"
	echo "    , --op_executable_file   Assign the path for optimization executable file"
	exit 1
}

if [ $# -lt 3 ]
then
	usage
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
	ADDITION_CONF_STR=""
fi

sp_executable_file=Release/minizero
op_executable_file=minizero/learner/train.py
while :; do
	case $1 in
		-h|--help) shift; usage
		;;
		-g|--gpu) shift; GPU_LIST=$1; NUM_GPU=${#GPU_LIST}
		;;
		-b|--batch_Size) shift; BATCH_SIZE=$1
		;;
		-c|--cpu_thread_per_gpu) shift; MAX_NUM_CPU_THREAD_PER_GPU=$1
		;;
		--conf_str) shift; ADDITION_CONF_STR=":$1"
		;;
		--sp_executable_file) shift; sp_executable_file=$1
		;;
		--op_executable_file) shift; op_executable_file=$1
		;;
		"") break
		;;
		*) echo "Unknown argument: $1"; usage
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
checkCommands=(${sp_executable_file} ${op_executable_file} rm flock kill nvidia-smi)

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
		rm -f $broker_fd
		closeFd $broker_fd
	fi
	exit
}

# kill all background process when exit
trap onExit SIGINT SIGTERM EXIT
trap true SIGALRM

retry_connection_counter=0
while true
do
	# try to connect to broker
	selfPlay_pid=""
	broker_fd=""
	exec {broker_fd}<>/dev/tcp/$HOST/$PORT
	if [[ -z $broker_fd ]]
	then
		# connect failed, retry at most 5 times
		max_retry_count=5
		retry_connection_counter=$(($retry_connection_counter+1))
		echo "connect to $HOST:$PORT failed, retry after 60s... ($retry_connection_counter/$max_retry_count)"
		if [ $retry_connection_counter -ge $max_retry_count ]
		then
			break
		fi
		sleep 60
	else
		echo "connect success"

		# send info
		NAME=$(hostname)"_"$GPU_LIST
		echo "Info $NAME $TYPE"
		echo "Info $NAME $TYPE" 1>&$broker_fd

		while true
		do
			# read from broker
			read -u $broker_fd line
			error_code=$?
			if [ $error_code -eq 0 ]
			then
				if [ "$line" == "keep_alive" ]
				then
					true
				elif [[ $line =~ ^Job_SelfPlay\ (.+) ]]
				then
					# format: Self-play train_dir conf_str
					var=(${BASH_REMATCH[1]})
					CONF_FILE=$(ls ${var[0]}/*.cfg)
					CONF_STR="${var[1]}:actor_num_threads=${NUM_CPU_THREAD}:actor_num_parallel_games=$((${BATCH_SIZE}*${NUM_GPU}))${ADDITION_CONF_STR}"
					CUDA_DEVICES=$(echo ${GPU_LIST} | awk '{ split($0, chars, ""); printf(chars[1]); for(i=2; i<=length(chars); ++i) { printf(","chars[i]); } }')
					echo "CUDA_VISIBLE_DEVICES=${CUDA_DEVICES} ${sp_executable_file} -mode sp -conf_file ${CONF_FILE} -conf_str \"${CONF_STR}\""
					CUDA_VISIBLE_DEVICES=${CUDA_DEVICES} ${sp_executable_file} -conf_file ${CONF_FILE} -conf_str "${CONF_STR}" -mode sp 0<&$broker_fd 1>&$broker_fd
				elif [[ $line =~ ^Job_Optimization\ (.+) ]]
				then
					var=(${BASH_REMATCH[1]})
					CONF_FILE=$(ls ${var[0]}/*.cfg)
					# py/Train.py train_dir model sgf_start sgf_end conf_file
					CUDA_DEVICES=$(echo ${GPU_LIST} | awk '{ split($0, chars, ""); printf(chars[1]); for(i=2; i<=length(chars); ++i) { printf(","chars[i]); } }')
					echo "CUDA_VISIBLE_DEVICES=${CUDA_DEVICES} PYTHONPATH=. python ${op_executable_file} ${var[0]} ${var[1]} ${var[2]} ${var[3]} ${CONF_FILE} 2>>${var[0]}/op.log"
					CUDA_VISIBLE_DEVICES=${CUDA_DEVICES} PYTHONPATH=. python ${op_executable_file} ${var[0]} ${var[1]} ${var[2]} ${var[3]} ${CONF_FILE} 1>&$broker_fd 2>>${var[0]}/op.log
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
done
