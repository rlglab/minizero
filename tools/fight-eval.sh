#!/usr/bin/bash
# check arguments
if [ $# -lt 6 ]
then
	echo "Usage: ./fight-eval.sh [Game type] [Folder1] [Folder2] [Config 1] [Interval] [Game num] [-s Start] [-f Config 2] [-b Board_Size] [-g GPU_LIST] [-d Result Folder Name]"
	exit 1
else
    GAME_TYPE=$1
    FOLDER1=$2
    FOLDER2=$3
    CONF_FILE1=$4
    INTERVAL=$5
    GAMENUM=$6
    shift 6
    # default arguments
    START=0
    CONF_FILE2=$CONF_FILE1
    NUM_GPU=$(nvidia-smi -L | wc -l)
    GPU_LIST=$(echo $NUM_GPU | awk '{for(i=0;i<$1;i++)printf i}')
    BOARD_SIZE=9
    NAME="${FOLDER1}_vs_${FOLDER2}_eval"
fi

while :; do
	case $1 in
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
		"") break
		;;
		*) echo "Unknown argument: $1"; exit 1
		;;
	esac
	shift
done

echo "./fight-eval.sh $GAME_TYPE $FOLDER1 $FOLDER2 $CONF_FILE1 $INTERVAL $GAMENUM -s $START -f $CONF_FILE2 -b $BOARD_SIZE -g $GPU_LIST -d $NAME"

if [ ! -d "${FOLDER1}" ] || [ ! -d "${FOLDER2}" ]; then
    echo "${FOLDER1} or ${FOLDER2} not exists!"
    exit 1
fi

if [ ! -d "${FOLDER1}/$NAME" ]; then
    mkdir "${FOLDER1}/$NAME"
fi
echo "FOLDERS: $FOLDER1 & $FOLDER2, CONF_FILES: $CONF_FILE1 & $CONF_FILE2 "
function run_twogtp(){
    BLACK="build/$GAME_TYPE/minizero_$GAME_TYPE -conf_file $CONF_FILE1 -conf_str \"nn_file_name=$FOLDER1/model/$2\""
    WHITE="build/$GAME_TYPE/minizero_$GAME_TYPE -conf_file $CONF_FILE2 -conf_str \"nn_file_name=$FOLDER2/model/$2\""
    EVAL_FOLDER="${FOLDER1}/$3/${2:12:-3}"
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
    CUDA_VISIBLE_DEVICES=$1 gogui-twogtp -black "$BLACK" -white "$WHITE" -games $GAMENUM -sgffile $SGFFILE -alternate -auto -size $BOARD_SIZE -komi $KOMI -threads 2
}
function run_gpu(){
    CUR_NUM=1
    # CNT=$INTERVAL # start from 0
    CNT=0  # start from INTERVAL
    S=0
    for file in $(ls -rt $FOLDER1/model | grep .pt )
    do
        if [ $S -lt $2 ];then
            S=$(($S+1))
            continue
        fi
        if [ $CNT -ne $INTERVAL ];then
            CNT=$(($CNT+1))
            continue
        fi
        CNT=1
        CUR_NUM=$(($CUR_NUM+1))
        P=$file
        run_twogtp $1 $P $3
    done
    echo "GPUID $1 done!"
}
for (( i=0; i < ${#GPU_LIST} ; i = i+1 ))
do
    GPUID=${GPU_LIST:$i:1}
    run_gpu $GPUID $START $NAME &
    sleep 10
done
wait
echo "done!"
