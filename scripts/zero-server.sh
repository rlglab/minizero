#!/bin/bash

set -e

# check argument
if [ $# -ne 3 ]; then
	echo "./zero-server.sh configure train_dir end_iteration"
	exit
fi

CONFIGURE_FILE=$1
TRAIN_DIR=$2
END_ITERATION=$3
CURRENT_PATH=`pwd`

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
	NEW_CONFIGURE_FILE=$(echo ${TRAIN_DIR} | awk -F "/" '{ print $NF".cfg"; }')
	cp ${CONFIGURE_FILE} ${TRAIN_DIR}/${NEW_CONFIGURE_FILE}

	# setup initial weight
	PYTHONPATH=. python minizero/learner/train.py ${TRAIN_DIR} "" -1 -1 ${TRAIN_DIR}/${NEW_CONFIGURE_FILE}
elif [[ ${run_stage} == "C" ]]; then
    ZERO_START_ITERATION=$(ls -t ${TRAIN_DIR}/sgf/* | head -n1 | sed 's/.sgf//g' | awk -F "/" '{ print $NF+1; }')
    MODEL_FILE=$(ls -t ${TRAIN_DIR}/model/*.pt | head -n1 | sed 's/\// /g' | awk '{ print $NF; }')
    NEW_CONFIGURE_FILE=$(ls ${TRAIN_DIR}/*.cfg | sed 's/\// /g' | awk '{ print $NF; }')
else
	exit
fi

# friendly notify
yn="?"
echo "Start training from iteration: ${ZERO_START_ITERATION}, model file: ${MODEL_FILE}, configuration: ${NEW_CONFIGURE_FILE}. Sure? (y/n)"
read -n1 yn
if [[ ${yn} != "y" ]]; then
    exit
fi

# run zero server
CONF_STR="zero_training_directory=${TRAIN_DIR}:zero_end_iteration=${END_ITERATION}:nn_file_name=${MODEL_FILE}:zero_start_iteration=${ZERO_START_ITERATION}"
#gdb --ex=r --args Release/minizero -conf_file ${TRAIN_DIR}/${NEW_CONFIGURE_FILE} -conf_str ${CONF_STR} -mode zero_server
Release/minizero -conf_file ${TRAIN_DIR}/${NEW_CONFIGURE_FILE} -conf_str ${CONF_STR} -mode zero_server
