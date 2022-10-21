#!/bin/bash
set -e

usage()
{
	echo "Usage: ./zero-server.sh configure_file train_dir end_iteration [OPTION...]"
	echo ""
	echo "  -h, --help                 Give this help list"
	echo "    , --sp_executable_file   Assign the path for self-play executable file"
	echo "    , --op_executable_file   Assign the path for optimization executable file"
	exit 1
}

# check argument
if [ $# -lt 3 ]; then
	usage
else
	CONFIGURE_FILE=$1
	TRAIN_DIR=$2
	END_ITERATION=$3
	shift
	shift
	shift
fi

sp_executable_file=Release/minizero
op_executable_file=minizero/learner/train.py
while :; do
	case $1 in
		-h|--help) shift; usage
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

run_stage="R"
if [ -d ${TRAIN_DIR} ]; then
	echo "${TRAIN_DIR} has existed: Restart(R) / Continue(C) / Quit(Q)."
	read -n1 run_stage
	echo ""
fi

ZERO_START_ITERATION=1
MODEL_FILE="weight_iter_0.pt"
if [[ ${run_stage} == "R" ]]; then
	rm -rf ${TRAIN_DIR}

	echo "create ${TRAIN_DIR} ..."
	mkdir ${TRAIN_DIR}
	mkdir ${TRAIN_DIR}/model
	mkdir ${TRAIN_DIR}/sgf
	touch ${TRAIN_DIR}/op.log
	NEW_CONFIGURE_FILE=$(echo ${TRAIN_DIR} | awk -F "/" '{ print ($NF==""? $(NF-1): $NF)".cfg"; }')
	cp ${CONFIGURE_FILE} ${TRAIN_DIR}/${NEW_CONFIGURE_FILE}

	# setup initial weight
	echo "\"\" -1 -1" | PYTHONPATH=. python ${op_executable_file} ${TRAIN_DIR} ${TRAIN_DIR}/${NEW_CONFIGURE_FILE} >/dev/null 2>&1
elif [[ ${run_stage} == "C" ]]; then
    ZERO_START_ITERATION=$(ls -t ${TRAIN_DIR}/sgf/* | head -n1 | sed 's/.sgf//g' | awk -F "/" '{ print $NF+1; }')
    MODEL_FILE=$(ls -t ${TRAIN_DIR}/model/*.pt | head -n1 | sed 's/\// /g' | awk '{ print $NF; }')
    NEW_CONFIGURE_FILE=$(ls ${TRAIN_DIR}/*.cfg | sed 's/\// /g' | awk '{ print $NF; }')
else
	exit
fi

# friendly notify
yn="?"
echo "Start training from iteration: ${ZERO_START_ITERATION}, model file: ${MODEL_FILE}, configuration: ${TRAIN_DIR}/${NEW_CONFIGURE_FILE}. Sure? (y/n)"
read -n1 yn
if [[ ${yn} != "y" ]]; then
    exit
fi

# run zero server
CONF_STR="zero_training_directory=${TRAIN_DIR}:zero_end_iteration=${END_ITERATION}:nn_file_name=${MODEL_FILE}:zero_start_iteration=${ZERO_START_ITERATION}"
${sp_executable_file} -conf_file ${TRAIN_DIR}/${NEW_CONFIGURE_FILE} -conf_str ${CONF_STR} -mode zero_server
