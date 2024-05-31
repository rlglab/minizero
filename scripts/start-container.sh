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
container_argumenets=""
record_history=false
while :; do
	case $1 in
		-h|--help) shift; usage
		;;
		--image) shift; image_name=${1}
		;;
		-v|--volume) shift; container_volume="${container_volume} -v ${1}"
		;;
		--name) shift; container_argumenets="${container_argumenets} --name ${1}"
		;;
		-d|--detach) container_argumenets="${container_argumenets} -d"
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

container_argumenets=$(echo ${container_argumenets} | xargs)
echo "$container_tool run ${container_argumenets} --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --network=host --ipc=host --rm -it ${container_volume} ${image_name}"
$container_tool run ${container_argumenets} --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --network=host --ipc=host --rm -it ${container_volume} ${image_name}
