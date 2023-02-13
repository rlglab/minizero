#!/usr/bin/bash
# check arguments
if [ $# -lt 5 ]
then
	echo "Usage: ./self-eval.sh [Game type] [Folder] [Config] [Interval] [Game num] [-s Start] [-b Board Size] [-g GPU LIST] [-d Result Folder Name]"
	exit 1
else
    GAME_TYPE=$1
    FOLDER=$2
    CONF_FILE=$3
    INTERVAL=$4
    GAMENUM=$5
    shift 5
    # default arguments
    START=0
    NUM_GPU=$(nvidia-smi -L | wc -l)
    GPU_LIST=$(echo $NUM_GPU | awk '{for(i=0;i<$1;i++)printf i}')
    BOARD_SIZE=9
    NAME="self_eval"
fi

while :; do
	case $1 in
		-g|--gpu) shift; GPU_LIST=$1; NUM_GPU=${#GPU_LIST}
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
echo "./self-eval.sh $GAME_TYPE $FOLDER $CONF_FILE $INTERVAL $GAMENUM -s $START -b $BOARD_SIZE -g $GPU_LIST -d $NAME"
if [ ! -d "${FOLDER}" ]; then
    echo "${FOLDER} not exists!"
    exit 1
fi

if [ ! -d "${FOLDER}/$NAME" ]; then
    mkdir "${FOLDER}/$NAME"
fi

function run_twogtp(){
    BLACK="build/$GAME_TYPE/minizero_$GAME_TYPE -conf_file $CONF_FILE -conf_str \"nn_file_name=$FOLDER/model/$3\""
    WHITE="build/$GAME_TYPE/minizero_$GAME_TYPE -conf_file $CONF_FILE -conf_str \"nn_file_name=$FOLDER/model/$2\""
    EVAL_FOLDER="${FOLDER}/$4/${3:12:-3}_vs_${2:12:-3}"
    SGFFILE="${EVAL_FOLDER}/${3:12:-3}_vs_${2:12:-3}"
    if [ ! -d "${EVAL_FOLDER}" ];then
        mkdir $EVAL_FOLDER
    fi
    if [ -f "$SGFFILE.lock" ] || [ -f "${SGFFILE}-$((${GAMENUM}-1)).sgf" ] ; then
        return
    fi
    KOMI=0
    if [[ $GAME_TYPE == go ]]; then
        KOMI=7
    fi
    echo "GPUID: $1, Current players: ${3:12:-3} vs. ${2:12:-3}, Game num $GAMENUM"
    CUDA_VISIBLE_DEVICES=$1 gogui-twogtp -black "$BLACK" -white "$WHITE" -games $GAMENUM -sgffile $SGFFILE -alternate -auto -size $BOARD_SIZE -komi $KOMI -threads 2
}
function run_gpu(){
    CUR_NUM=1
    CNT=1
    S=0
    P1=""
    P2=""
    for file in $(ls -rt $FOLDER/model | grep .pt )
    do
        if [ $S -lt $2 ];then
            S=$(($S+1))
            continue
        fi
        if [ $S -eq $2 ];then
            P1=$file
            S=$(($S+1))
            continue
        fi
        if [ $CNT -ne $INTERVAL ];then
            CNT=$(($CNT+1))
            continue
        fi
        CNT=1
        CUR_NUM=$(($CUR_NUM+1))
        P2=$file
        run_twogtp $1 $P1 $P2 $3
        P1=$P2
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
