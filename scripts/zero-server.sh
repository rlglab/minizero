#!/bin/bash
set -e

env_cmakelists="$(dirname $(readlink -f "$0"))/../minizero/environment/CMakeLists.txt"
support_games=($(awk '/target_include_directories/,/\)/' ${env_cmakelists} | sed 's|/|\n|g' | grep -v -E 'target|environment|PUBLIC|CMAKE_CURRENT_SOURCE_DIR|base|stochastic|)'))

usage()
{
	echo "Usage: $0 GAME_TYPE CONFIGURE_FILE END_ITERATION [OPTION]..."
	echo "The zero-server manages the training session and organizes connected zero-workers."
	echo ""
	echo "Required arguments:"
    echo "  GAME_TYPE: ${support_games[@]}"
	echo "  CONFIGURE_FILE: the configure file (*.cfg) to use"
	echo "  END_ITERATION: the total number of iterations for training"
	echo ""
	echo "Optional arguments:"
	echo "  -h,        --help                 Give this help list"
	echo "  -n,        --name                 Assign name for training directory"
	echo "  -np,       --name_prefix          Add prefix name for default training directory name"
	echo "  -ns,       --name_suffix          Add suffix name for default training directory name"
	echo "  -g,        --gpu                  Assign the GPU for network model initialization, e.g. 0"
	echo "             --sp_executable_file   Assign the path for self-play executable file"
	echo "             --op_executable_file   Assign the path for optimization executable file"
	echo "             --link_sgf             Assign the path of sgf for training without self play (only op)"
	echo "  -conf_str                         Overwrite settings in the configure file"
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
gpu_list=$(nvidia-smi --query-gpu=index,memory.used,utilization.gpu --format=csv,noheader,nounits | sort -k2n -k3n | head -1 | cut -d, -f1)
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
		-g|--gpu) shift; gpu_list=$1
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

# create default name; also check if configurations are valid
testrun_stderr_tmp=$(mktemp)
default_name=$(${sp_executable_file} -mode zero_training_name -conf_file ${configure_file} -conf_str "${overwrite_conf_str}" 2>${testrun_stderr_tmp} || :)
testrun_stderr=$(<${testrun_stderr_tmp})
rm -f ${testrun_stderr_tmp}
if [[ ! ${default_name} ]]; then
	echo "${testrun_stderr}" >&2
	exit 1
fi
# use default name of training if name is not assigned
if [[ -z ${train_dir} ]]; then
	train_dir=${name_prefix}${default_name}${name_suffix}
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
		end_iteration=$(ls ${train_dir}/sgf/ | wc -l)
		echo "link ${link_sgf} ..."
		echo "end_iteration: ${end_iteration}"
	fi
	touch ${train_dir}/op.log
	new_configure_file=$(basename ${train_dir}).cfg
	${sp_executable_file} -gen ${train_dir}/${new_configure_file} -conf_file ${configure_file} -conf_str "${overwrite_conf_str}" 2>/dev/null

	# keep a tagged backup of the config used from this starting iteration
	mkdir -p ${train_dir}/cfg
	cp ${train_dir}/${new_configure_file} ${train_dir}/cfg/${new_configure_file%.cfg}_${zero_start_iteration}.cfg

	# setup initial weight
	cuda_devices=$(echo ${gpu_list} | awk '{ split($0, chars, ""); printf(chars[1]); for(i=2; i<=length(chars); ++i) { printf(","chars[i]); } }')
	echo "train \"\" -1 -1" | CUDA_VISIBLE_DEVICES=${cuda_devices} PYTHONPATH=. python ${op_executable_file} ${game_type} ${train_dir} ${train_dir}/${new_configure_file} >/dev/null 2>&1
elif [[ ${run_stage,} == "c" ]]; then
	new_configure_file=$(basename ${train_dir}/*.cfg)

	# resolve the requested starting iteration (-conf_str takes precedence over the config file);
	# 0 (or unset) means continue from the latest model, a positive number loads that iteration
	if [[ ":${overwrite_conf_str}:" == *":zero_start_iteration="* ]]; then
		req_start_iteration=$(echo ":${overwrite_conf_str}:" | grep -oE 'zero_start_iteration=[^:]*' | tail -n1 | cut -d= -f2 | tr -d '[:space:]')
	else
		req_start_iteration=$(grep -E "^[[:space:]]*zero_start_iteration=" ${configure_file} | tail -n1 | cut -d= -f2 | cut -d'#' -f1 | tr -d '[:space:]')
	fi

	models=($(ls ${train_dir}/model/ 2>/dev/null | grep "\.pt$" | sort -V))
	if [[ ${#models[@]} -eq 0 ]]; then
		echo "Error: Model files not found! No models found in ${train_dir}/model/."
		exit 1
	fi

	if [[ -z ${req_start_iteration} || ${req_start_iteration} -eq 0 ]]; then
		# 0 (or unset): continue from the latest existing model
		zero_start_iteration=${#models[@]}
		model_file=${models[$((${#models[@]}-1))]}
	else
		# positive number: start from the given iteration and load its corresponding model
		zero_start_iteration=${req_start_iteration}
		msg_load_step=$((${zero_start_iteration}-1))
		if [[ ${msg_load_step} -ge ${#models[@]} ]]; then
			echo "Start from iteration: ${zero_start_iteration}, configuration: ${train_dir}/${new_configure_file}."
			echo "Error: Model files not found! Expected model at index ${msg_load_step}, but only ${#models[@]} models found."
			exit 1
		fi
		model_file=${models[${msg_load_step}]}
	fi

	# generate the in-dir config from the provided configure_file (overwriting the old one), forcing the
	# resolved starting iteration through -conf_str (which overrides the file's value)
	gen_conf_str="zero_start_iteration=${zero_start_iteration}"
	[[ -n ${overwrite_conf_str} ]] && gen_conf_str="${gen_conf_str}:${overwrite_conf_str}"
	echo y | ${sp_executable_file} -gen ${train_dir}/${new_configure_file} -conf_file ${configure_file} -conf_str "${gen_conf_str}" 2>/dev/null

	# keep a tagged backup of the config used from this starting iteration
	mkdir -p ${train_dir}/cfg
	cp ${train_dir}/${new_configure_file} ${train_dir}/cfg/${new_configure_file%.cfg}_${zero_start_iteration}.cfg

	read -n1 -p "Start from iteration: ${zero_start_iteration}, model file: ${model_file}, configuration: ${train_dir}/${new_configure_file}. Sure? (y/n) " yn
	[[ ${yn,,} == "y" ]] || exit
	echo ""

	# collect models at or after the starting iteration. files are named weight_iter_<training_step>
	# (not by iteration), but the -V sorted models[] index equals the iteration number, so select by
	# index here. remove both the .pt (self-play) and matching .pkl (training snapshot); they are regenerated
	models_to_remove=()
	for idx in "${!models[@]}"; do
		if [[ ${idx} -ge ${zero_start_iteration} ]]; then
			pt_path="${train_dir}/model/${models[$idx]}"
			pkl_path="${train_dir}/model/${models[$idx]%.pt}.pkl"
			[[ -e ${pt_path} ]] && models_to_remove+=("${pt_path}")
			[[ -e ${pkl_path} ]] && models_to_remove+=("${pkl_path}")
		fi
	done
	last_model_iteration=$((${#models[@]}-1))

	# collect stale config backups: those tagged with an iteration greater than the starting one.
	# the backup tagged == starting iteration was just created above and is kept
	cfg_backup_base="${train_dir}/cfg/${new_configure_file%.cfg}"
	cfgs_to_remove=()
	for cfg_path in ${cfg_backup_base}_*.cfg; do
		[[ -e ${cfg_path} ]] || continue
		cfg_iter=${cfg_path##*_}
		cfg_iter=${cfg_iter%.cfg}
		[[ ${cfg_iter} =~ ^[0-9]+$ ]] || continue
		[[ ${cfg_iter} -gt ${zero_start_iteration} ]] && cfgs_to_remove+=("${cfg_path}")
	done

	# locate the first line for the starting iteration in each log; everything from it onward is stale
	log_files=("${train_dir}/Training.log" "${train_dir}/op.log")
	logs_to_trim=()
	log_cut_lines=()
	for log_file in "${log_files[@]}"; do
		[[ -f ${log_file} ]] || continue
		cut_line=$(grep -Fn "[Iteration] =====${zero_start_iteration}=====" ${log_file} | head -n1 | cut -d: -f1)
		if [[ -n ${cut_line} ]]; then
			logs_to_trim+=("${log_file}")
			log_cut_lines+=("${cut_line}")
		fi
	done

	# ask before removing anything
	if [[ ${#models_to_remove[@]} -gt 0 || ${#cfgs_to_remove[@]} -gt 0 || ${#logs_to_trim[@]} -gt 0 ]]; then
		echo "The following will be removed to restart from iteration ${zero_start_iteration}:"
		if [[ ${#models_to_remove[@]} -gt 0 ]]; then
			echo "  - model files: iteration ${zero_start_iteration} to ${last_model_iteration} (${#models_to_remove[@]} files, .pt + .pkl)"
		fi
		if [[ ${#cfgs_to_remove[@]} -gt 0 ]]; then
			echo "  - stale config backup(s):"
			printf '      %s\n' "${cfgs_to_remove[@]}"
		fi
		for idx in "${!logs_to_trim[@]}"; do
			echo "  - $(basename ${logs_to_trim[$idx]}): from line ${log_cut_lines[$idx]} onward"
		done
		read -n1 -p "Proceed with removal? (y/n) " del_yn
		echo ""
		if [[ ${del_yn,,} == "y" ]]; then
			if [[ ${#models_to_remove[@]} -gt 0 ]]; then
				rm -f "${models_to_remove[@]}"
				echo "removed ${#models_to_remove[@]} model file(s)."
			fi
			if [[ ${#cfgs_to_remove[@]} -gt 0 ]]; then
				rm -f "${cfgs_to_remove[@]}"
				echo "removed ${#cfgs_to_remove[@]} config backup(s)."
			fi
			for idx in "${!logs_to_trim[@]}"; do
				log_file="${logs_to_trim[$idx]}"
				cut_line="${log_cut_lines[$idx]}"
				head -n $((cut_line-1)) ${log_file} > ${log_file}.tmp && mv ${log_file}.tmp ${log_file}
				echo "trimmed $(basename ${log_file}) to before iteration ${zero_start_iteration}."
			done
		else
			echo "keep all models, config backups, and logs; skip removal."
		fi
	fi
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
