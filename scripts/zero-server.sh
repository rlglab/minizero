#!/bin/bash
set -e

usage()
{
	echo "Usage: ./zero-server.sh game_type configure_file end_iteration [OPTION...]"
	echo ""
	echo "  -h        , --help                 Give this help list"
	echo "  -n        , --name                 Assign name for training directory"
	echo "  -np       , --name_prefix          Add prefix name for default training directory name"
	echo "  -ns       , --name_suffix          Add suffix name for default training directory name"
	echo "            , --sp_executable_file   Assign the path for self-play executable file"
	echo "            , --op_executable_file   Assign the path for optimization executable file"
	echo "            , --link_sgf   	   	   Assign the path of sgf for training without self play (only op)"
	echo "  -conf_str ,                        Overwrite configuration file"
	exit 1
}

# check argument
if [ $# -lt 3 ] || [ $(($# % 2)) -eq 0 ]; then
	usage
else
	game_type=$1; shift
	configure_file=$1; shift
	end_iteration=$1; shift
fi

train_dir=""
name_prefix=""
name_suffix=""
sp_executable_file=build/${game_type}/minizero_${game_type}
op_executable_file=minizero/learner/train.py
overwrite_conf_str=""
link_sgf=""
while :; do
	case $1 in
		-h|--help) shift; usage
		;;
		-n|--name) shift; train_dir=$1
		;;
		-np|--name_prefix) shift; name_prefix=$1
		;;
		-ns|--name_suffix) shift; name_suffix=$1
		;;
		--sp_executable_file) shift; sp_executable_file=$1
		;;
		--op_executable_file) shift; op_executable_file=$1
		;;
		--link_sgf) shift; link_sgf=$1
		;;
		-conf_str) shift; overwrite_conf_str=$1
		;;
		"") break
		;;
		*) echo "Unknown argument: $1"; usage
		;;
	esac
	shift
done

# create default name of training if name is not assigned
if [[ -z ${train_dir} ]]; then
	train_dir=${name_prefix}$(${sp_executable_file} -mode zero_training_name -conf_file ${configure_file} -conf_str "${overwrite_conf_str}" 2>/dev/null)${name_suffix}
fi

run_stage="R"
if [ -d ${train_dir} ]; then
	read -n1 -p "${train_dir} has existed. (R)estart / (C)ontinue / (Q)uit? " run_stage
	echo ""
fi

zero_start_iteration=1
model_file="weight_iter_0.pt"
if [[ ${run_stage,} == "r" ]]; then
	rm -rf ${train_dir}

	echo "create ${train_dir} ..."
	mkdir -p ${train_dir}/model ${train_dir}/sgf
	if [[ ! -z ${link_sgf} ]];
	then
		ln ${link_sgf}/* ${train_dir}/sgf/
		echo "link ${link_sgf} ..."
	fi
	touch ${train_dir}/op.log
	new_configure_file=$(basename ${train_dir}).cfg
	${sp_executable_file} -gen ${train_dir}/${new_configure_file} -conf_file ${configure_file} -conf_str "${overwrite_conf_str}" 2>/dev/null

	# setup initial weight
	echo "train \"\" -1 -1" | PYTHONPATH=. python ${op_executable_file} ${game_type} ${train_dir} ${train_dir}/${new_configure_file} >/dev/null 2>&1
elif [[ ${run_stage,} == "c" ]]; then
    zero_start_iteration=$(ls ${train_dir}/model/ | grep ".pt$" | wc -l)
    model_file=$(ls ${train_dir}/model/ | grep ".pt$" | sort -V | tail -n1)
    new_configure_file=$(basename ${train_dir}/*.cfg)
	echo y | ${sp_executable_file} -gen ${train_dir}/${new_configure_file} -conf_file ${train_dir}/${new_configure_file} -conf_str "${overwrite_conf_str}" 2>/dev/null

	# friendly notification if continuing training
	read -n1 -p "Continue training from iteration: ${zero_start_iteration}, model file: ${model_file}, configuration: ${train_dir}/${new_configure_file}. Sure? (y/n) " yn
	[[ ${yn,,} == "y" ]] || exit
	echo ""
else
	exit
fi

# run zero server
conf_str="zero_training_directory=${train_dir}:zero_end_iteration=${end_iteration}:nn_file_name=${model_file}:zero_start_iteration=${zero_start_iteration}"
if [[ ! -z ${link_sgf} ]];
then
	conf_str="$conf_str:zero_num_games_per_iteration=0"
fi
${sp_executable_file} -conf_file ${train_dir}/${new_configure_file} -conf_str ${conf_str} -mode zero_server
