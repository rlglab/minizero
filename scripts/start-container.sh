#!/bin/bash
set -e

usage()
{
	echo "Usage: $0 [OPTION]..."
	echo ""
	echo "Optional arguments:"
	echo "  -h, --help     Give this help list"
	echo "      --image    Select the image name of the container"
	echo "  -v, --volume   Bind mount a volume into the container"
	echo "      --name     Assign a name to the container"
	echo "  -d, --detach   Run container in background and print container ID"
	echo "  -H, --history  Record the container bash history"
	exit 1
}

image_name=kds285/minizero:latest
container_tool=$(basename $(which podman || which docker) 2>/dev/null)
if [[ ! $container_tool ]]; then
	echo "Neither podman nor docker is installed." >&2
	exit 1
fi
container_volume="-v .:/workspace"
container_arguments=""
record_history=false
while :; do
	case $1 in
		-h|--help) shift; usage
		;;
		--image) shift; image_name=${1}
		;;
		-v|--volume) shift; container_volume="${container_volume} -v ${1}"
		;;
		--name) shift; container_arguments="${container_arguments} --name ${1}"
		;;
		-d|--detach) container_arguments="${container_arguments} -d"
		;;
		-H|--history) record_history=true
		;;
		"") break
		;;
		*) echo "Unknown argument: $1"; usage
		;;
	esac
	shift
done

# check GPU environment
if ! command -v nvidia-smi &> /dev/null; then
	echo "Error: nvidia-smi not found. Please ensure NVIDIA drivers are installed and available in PATH." >&2
	exit 1
fi

if ! command -v nvidia-container-cli &> /dev/null; then
	echo "Error: 'nvidia-container-cli' not found. NVIDIA Container Toolkit is likely not installed." >&2
	echo "Visit https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/install-guide.html" >&2
	exit 1
fi

if [ "$record_history" = true ]; then
	history_dir=".container_root"
	# if the history directory is not exist, create it and initialize the history directory
	if [ ! -d ${history_dir} ]; then
		mkdir -p ${history_dir}
		# start container with the history directory and copy the history to the history directory
		$container_tool run --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --rm -it -v "${history_dir}:/container_root" ${image_name} /bin/bash -c "cp -r /root/. /container_root && touch /container_root/.bash_history && exit"
	fi
	# add the history directory to the container volume
	container_volume="${container_volume} -v ${history_dir}:/root"
fi

container_arguments=$(echo ${container_arguments} | xargs)

if [ ${container_tool} = "podman" ]; then
	echo "$container_tool run ${container_arguments} --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --network=host --ipc=host --rm -it ${container_volume} ${image_name}"
	$container_tool run ${container_arguments} --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --network=host --ipc=host --rm -it ${container_volume} ${image_name}
elif [ ${container_tool} = "docker" ]; then
	# manually mount GPU devices into Docker to resolve 'Failed to Initialize NVML: Unknown Error'
	device_args="--gpus all"
	for path in /dev/nvidia*; do
		if [ -c ${path} ]; then
			device_args+=" --device=${path}"
		fi
	done
	# add Git safe directory
	git_safe_cmd='git config --global --add safe.directory /workspace && exec bash'
	# add argument
	container_arguments="${container_arguments} -e container=${container_tool}"
	echo "$container_tool run ${container_arguments} ${device_args} --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --network=host --ipc=host --rm -it ${container_volume} ${image_name} bash -c \"${git_safe_cmd}\""
	$container_tool run ${container_arguments} ${device_args} --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --network=host --ipc=host --rm -it ${container_volume} ${image_name} bash -c "$git_safe_cmd"
fi
