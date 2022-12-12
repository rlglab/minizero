#!/bin/bash

usage()
{
	echo "Usage: ./zero-env.sh host port [OPTION...]"
	echo ""
	echo "  -h, --help                 Give this help list"
	echo "  -c, --cpu_thread           Assign the number of CPUs for running environment (default = 32)"
	echo "    , --executable_file      Assign the path for executable file"
	exit 1
}

if [ $# -lt 2 ]
then
	usage
else
	HOST=$1
	PORT=$2
	shift
	shift
	
	# default arguments
	NUM_CPU_THREAD=32
fi

executable_file=minizero/actor/py/atari_env.py
while :; do
	case $1 in
		-h|--help) shift; usage
		;;
		-c|--cpu_thread) shift; NUM_CPU_THREAD=$1
		;;
		--conf_str) shift; ADDITION_CONF_STR=":$1"
		;;
		--executable_file) shift; executable_file=$1
		;;
		"") break
		;;
		*) echo "Unknown argument: $1"; usage
		;;
	esac
	shift
done

# every command in checkCommands must be executable
checkCommands=(${EXECUTABLE_FILE})

for name in "${checkCommands[@]}"
do
	which $name > /dev/null
	if [ $? -ne 0 ]
	then
		echo "Cannot run $name, exit."
		exit 1
	fi
done

broker_fd=""
exec {broker_fd}<>/dev/tcp/$HOST/$PORT
if [[ -z $broker_fd ]]
then
	exit
fi

read -u $broker_fd train_dir
error_code=$?
if [ $error_code -eq 0 ]
then
	CONF_FILE=$(ls ${train_dir}/*.cfg)
	ENV_NAME=$(grep "^env_atari_name=" atari/atari.cfg | tail -n1 | cut -d '=' -f 2)

	# format: python atair_env.py ALE/MsPacman-v5 32
	echo "python ${executable_file} ${ENV_NAME} ${NUM_CPU_THREAD}"
	python ${executable_file} ${ENV_NAME} ${NUM_CPU_THREAD} 0<&$broker_fd 1>&$broker_fd
fi
