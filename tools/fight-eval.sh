#!/bin/bash
set -e

usage()
{
	echo "Usage: ./fight-eval.sh GAME_TYPE FOLDER1 FOLDER2 CONF_FILE1 INTERVAL GAMENUM [OPTION...]"
	echo ""
	echo "  -h        , --help                 Give this help list"
    echo "  -s                                 Start from which file in the folder (default 0)"
	echo "  -f                                 The configure file of FOLDER2 (default Config 1)"
	echo "  -b                                 Board size (default 9)"
	echo "  -g, --gpu                          Assign available GPUs, e.g. 0123"
    echo "            , --num_threads          Number of threads to play games"
	echo "  -d                                 Result Folder Name (default [Folder1]_vs_[Folder2]_eval)"
	echo "            , --sp_executable_file   Assign the path for fighting executable file (default 2)"
	exit 1
}

# check arguments
if [ $# -lt 6 ] || [ $(($# % 2)) -eq 1 ];
then
	usage
else
    GAME_TYPE=$1; shift
    FOLDER1=$1; shift
    FOLDER2=$1; shift
    CONF_FILE1=$1; shift
    INTERVAL=$1; shift
    GAMENUM=$1; shift
fi

# default arguments
START=0
CONF_FILE2=$CONF_FILE1
NUM_GPU=$(nvidia-smi -L | wc -l)
GPU_LIST=$(echo $NUM_GPU | awk '{for(i=0;i<$1;i++)printf i}')
num_threads=2
BOARD_SIZE=9
NAME="$(basename ${FOLDER1})_vs_$(basename ${FOLDER2})_eval"
sp_executable_file=build/${GAME_TYPE}/minizero_${GAME_TYPE}
while :; do
	case $1 in
        -h|--help) shift; usage
		;;
		-g|--gpu) shift; GPU_LIST=$1; NUM_GPU=${#GPU_LIST}
		;;
        -f) shift; CONF_FILE2=$1
        ;;
        -b) shift; BOARD_SIZE=$1
		;;
        -s) shift; START=$1
		;;
        -d) shift; NAME=$1
		;;
        --num_threads) shift; num_threads=$1
        ;;
        --sp_executable_file) shift; sp_executable_file=$1
		;;
		"") break
		;;
		*) echo "Unknown argument: $1"; usage
		;;
	esac
	shift
done

echo "./fight-eval.sh $GAME_TYPE $FOLDER1 $FOLDER2 $CONF_FILE1 $INTERVAL $GAMENUM -s $START -f $CONF_FILE2 -b $BOARD_SIZE -g $GPU_LIST -d $NAME --num_threads $num_threads --sp_executable_file $sp_executable_file"

if [ ! -d "${FOLDER1}" ] || [ ! -d "${FOLDER2}" ]; then
    echo "${FOLDER1} or ${FOLDER2} not exists!"
    exit 1
fi

if [ ! -d "${FOLDER1}/$NAME" ]; then
    mkdir "${FOLDER1}/$NAME"
fi
echo "FOLDERS: $FOLDER1 & $FOLDER2, CONF_FILES: $CONF_FILE1 & $CONF_FILE2 "
function run_twogtp(){
    BLACK="$sp_executable_file -conf_file $CONF_FILE1 -conf_str \"nn_file_name=$FOLDER1/model/$2\""
    WHITE="$sp_executable_file -conf_file $CONF_FILE2 -conf_str \"nn_file_name=$FOLDER2/model/$2\""
    EVAL_FOLDER="${FOLDER1}/$NAME/${2:12:-3}"
    SGFFILE="${EVAL_FOLDER}/${2:12:-3}"
    if [ -f "$SGFFILE.lock" ] || [ -f "${SGFFILE}-$((${GAMENUM}-1)).sgf" ] || [ ! -f "$FOLDER2/model/$2" ] || [ ! -f "$FOLDER1/model/$2" ] ; then
        return
    fi
    
    if [ ! -d "${EVAL_FOLDER}" ];then
        mkdir $EVAL_FOLDER
    fi
    KOMI=0
    if [[ $GAME_TYPE == go ]]; then
        KOMI=7
    fi
    echo "GPUID: $1, Current players: ${2:12:-3}, Game num $GAMENUM"
    CUDA_VISIBLE_DEVICES=$1 gogui-twogtp -black "$BLACK" -white "$WHITE" -games $GAMENUM -sgffile $SGFFILE -alternate -auto -size $BOARD_SIZE -komi $KOMI -threads $num_threads
}
function run_gpu(){
    models=($(ls $FOLDER1/model | grep ".pt$" | sort -V))
    for((i=$START;i<${#models[@]};i=i+$INTERVAL))
    do
        run_twogtp $1 ${models[$i]}
    done
    echo "GPUID $1 done!"
}
for (( i=0; i < ${#GPU_LIST} ; i = i+1 ))
do
    GPUID=${GPU_LIST:$i:1}
    run_gpu $GPUID &
    sleep 10
done
wait
echo "All done!"
