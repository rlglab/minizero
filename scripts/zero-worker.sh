#!/bin/bash

usage()
{
	echo "Usage: $0 GAME_TYPE HOST PORT WORKER_TYPE [OPTION]..."
	echo "The zero-worker connects to a zero-server and performs either self-play or optimization."
	echo ""
	echo "Required arguments:"
	echo "  GAME_TYPE: $(find ./ ../ -maxdepth 2 -name build.sh -exec grep -m1 support_games {} \; -quit | sed -E 's/.+\("|"\).*//g;s/" "/, /g')"
	echo "  HOST, PORT: the host and port to connect the zero-server"
	echo "  WORKER_TYPE: sp, op"
	echo ""
	echo "Optional arguments:"
	echo "  -h,        --help                 Give this help list"
	echo "  -g,        --gpu                  Assign available GPUs for worker, e.g. 0123"
	echo "  -b,        --batch_size           Assign the batch size in self-play worker (default = 64)"
	echo "  -c,        --cpu_thread_per_gpu   Assign the number of CPUs for each GPU in self-play worker (default = 4)"
	echo "  -conf_str                         Add additional configure string in self-play worker"
	echo "             --sp_executable_file   Assign the path for self-play executable file"
	echo "             --op_executable_file   Assign the path for optimization executable file"
	exit 1
}

if [ $# -lt 4 ]
then
	usage
else
	game_type=$1; shift
	host=$1; shift
	port=$1; shift
	worker_type=$1; shift
	
	# default arguments
	num_gpu=$(nvidia-smi -L | wc -l)
	gpu_list=$(echo $num_gpu | awk '{for(i=0;i<$1;i++)printf i}')
	batch_size=64
	max_num_cpu_thread_per_gpu=4
	additional_conf_str=""
fi

sp_executable_file=build/${game_type}/minizero_${game_type}
op_executable_file=minizero/learner/train.py
while :; do
	case $1 in
		-h|--help) shift; usage
		;;
		-g|--gpu) shift; gpu_list=$1; num_gpu=${#gpu_list}
		;;
		-b|--batch_Size) shift; batch_size=$1
		;;
		-c|--cpu_thread_per_gpu) shift; max_num_cpu_thread_per_gpu=$1
		;;
		-conf_str) shift; additional_conf_str=":$1"
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
cuda_devices=$(echo ${gpu_list} | awk '{ split($0, chars, ""); printf(chars[1]); for(i=2; i<=length(chars); ++i) { printf(","chars[i]); } }')
max_num_cpu_thread=$((max_num_cpu_thread_per_gpu*num_gpu))
num_cpu_thread=$(lscpu -p=CORE | grep -v "#" | wc -l)
if [ $num_cpu_thread -gt $max_num_cpu_thread ]; then
	num_cpu_thread=$max_num_cpu_thread
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
	exec {broker_fd}<>/dev/tcp/$host/$port
	if [[ -z $broker_fd ]]
	then
		# connect failed, retry at most 5 times
		max_retry_count=5
		retry_connection_counter=$(($retry_connection_counter+1))
		echo "connect to $host:$port failed, retry after 60s... ($retry_connection_counter/$max_retry_count)"
		if [ $retry_connection_counter -ge $max_retry_count ]
		then
			break
		fi
		sleep 60
	else
		echo "connect success"
		retry_connection_counter=0

		# send info
		NAME=$(hostname)"_"$gpu_list
		echo "Info $NAME $worker_type"
		echo "Info $NAME $worker_type" 1>&$broker_fd

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
					CONF_STR="${var[1]}:zero_training_directory=${var[0]}:zero_num_threads=${num_cpu_thread}:zero_num_parallel_games=$((${batch_size}*${num_gpu}))${additional_conf_str}"
					echo "CUDA_VISIBLE_DEVICES=${cuda_devices} ${sp_executable_file} -mode sp -conf_file ${CONF_FILE} -conf_str \"${CONF_STR}\""
					CUDA_VISIBLE_DEVICES=${cuda_devices} ${sp_executable_file} -conf_file ${CONF_FILE} -conf_str "${CONF_STR}" -mode sp 0<&$broker_fd 1>&$broker_fd
				elif [[ $line =~ ^Job_Optimization\ (.+) ]]
				then
					var=(${BASH_REMATCH[1]})
					CONF_FILE=$(ls ${var[0]}/*.cfg)
					# format: py/Train.py train_dir conf_file
					echo "CUDA_VISIBLE_DEVICES=${cuda_devices} PYTHONPATH=. python ${op_executable_file} ${game_type} ${var[0]} ${CONF_FILE}"
					CUDA_VISIBLE_DEVICES=${cuda_devices} PYTHONPATH=. python ${op_executable_file} ${game_type} ${var[0]} ${CONF_FILE} 0<&$broker_fd 1>&$broker_fd 2> >(tee -a ${var[0]}/op.log >&2)
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
