#!/bin/bash

usage() {
    case "$1" in
    remove_obs)
        echo "Usage: $0 remove_obs SGF_DIR|SGF_FILE [NUM_THREADS]"
        echo "Remove all observation in the sgf files with tag \"OBS\"."
        echo ""
        echo "Required arguments:"
        echo "  SGF_DIR|SGF_FILE: sgf directory with all sgf files, or a specific sgf file"
        echo ""
        echo "Optional arguments:"
        echo "  NUM_THREADS: how many threads to use"
        echo "  -h,        --help   show this help message and exit"
        ;;
    recover_obs)
        echo "Usage: $0 recover_obs SGF_DIR|SGF_FILE CONF_FILE [NUM_THREADS]"
        echo "Recover all observation in the sgf files with tag \"OBS\"."
        echo ""
        echo "Required arguments:"
        echo "  SGF_DIR|SGF_FILE: sgf directory with all sgf files, or a specific sgf file"
        echo "  CONF_FILE: the configure file (*.cfg) used during training"
        echo ""
        echo "Optional arguments:"
        echo "  NUM_THREADS: how many threads to use"
        echo "  -h,        --help   show this help message and exit"
        ;;
    *)
        echo "Usage: $0 MODE [OPTION]..."
        echo "Handle obs by MODE. (need a ./build/atari/minizero_atari executable)"
        echo ""
        echo "Required arguments:"
        echo "  MODE: remove_obs, recover_obs"
        echo ""
        echo "Use $0 MODE -h for usage of each MODE."
        ;;
    esac
}

if [[ ! -f ./build/atari/minizero_atari ]]; then
    usage
    exit 1
fi

mode=$1; shift
if [[ $mode == remove_obs ]]; then # ======================================= REMOVE OBS =======================================
    if [[ $# < 1 ]]; then # not enough arguments
        usage $mode
        exit 1
    fi

    sgf_path=$1; shift

    if [[ $sgf_path =~ ^(|-h|--help)$ ]]; then # help format "./handle_obs.sh remove_obs -h"
        usage $mode
        exit 0
    fi

    num_threads=4
    if [[ $# > 0 ]]; then # set the number of threads
        num_threads=$1; shift
    fi

    if [[ ${sgf_path: -4} != ".sgf" ]]; then
        max_id=$(ls $sgf_path/[0-9]*\.sgf | wc -l) # count the number of sgf files
    fi

    echo $sgf_path | ./build/atari/minizero_atari -conf_str "zero_num_threads=$num_threads" -mode remove_obs 2> /dev/null # remove obs
    if [[ ${sgf_path: -4} == ".sgf" ]]; then
        # remove obs of a specific sgf file
        remove_obs_path=${sgf_path::-4}_remove_obs.sgf
        if [ ! -f $remove_obs_path ]; then
            # improper game (minizero_atari does not generate remove_obs.sgf)
            # do nothing
            echo > /dev/null
        else
            touch -r $sgf_path $remove_obs_path # reset time stamp ({id}_remove_obs.sgf reference to {id}.sgf)
            rm $sgf_path # remove {id}.sgf
        fi
    else
        # remove obs of a directory with all sgf files
        if [[ $max_id == 0 ]]; then
            echo "There is no any sgf files in the $sgf_path"
            exit 1
        fi

        if [[ $(ls ${sgf_path} -l | grep "^-" | wc -l) == $max_id ]]; then
            # improper game (minizero_atari does not generate remove_obs.sgf)
            # do nothing
            echo > /dev/null
        else
            for i in $(seq 1 $max_id)
            do
                touch -r $sgf_path/${i}.sgf $sgf_path/${i}_remove_obs.sgf # reset time stamp ({id}_remove_obs.sgf reference to {id}.sgf)
            done

            find $sgf_path -regex '.*/[0-9]+\.sgf' -delete # remove all {id}.sgf
        fi
    fi

elif [[ $mode == recover_obs ]]; then # ======================================= RECOVER OBS =======================================
    if [[ $# < 2 ]]; then # not enough arguments
        usage $mode
        exit 1
    fi

    sgf_path=$1; shift

    if [[ $sgf_path =~ ^(|-h|--help)$ ]]; then # help format "./handle_obs.sh recover_obs -h"
        usage $mode
        exit 0
    fi

    conf_file=$1; shift
    num_threads=-1
    if [[ $# > 0 ]]; then # set the number of threads
        num_threads=$1; shift
    fi

    if [[ ${sgf_path: -4} != ".sgf" ]]; then
        max_id=$(ls $sgf_path/[0-9]*_remove_obs\.sgf | wc -l) # count the number of sgf files
    fi

    if [[ $num_threads == -1 ]]; then
        echo $sgf_path | ./build/atari/minizero_atari -conf_file $conf_file -mode recover_obs 2> /dev/null
    else
        echo $sgf_path | ./build/atari/minizero_atari -conf_file $conf_file -conf_str "zero_num_threads=$num_threads" -mode recover_obs 2> /dev/null
    fi

    if [[ ${sgf_path: -4} == ".sgf" ]]; then
        # recover obs of a specific sgf file
        touch -r $sgf_path ${sgf_path::-15}.sgf # reset time stamp ({id}.sgf reference to {id}_remove_obs.sgf)
        rm $sgf_path # remove {id}_remove_obs.sgf
    else
        # recover obs of a directory with all sgf files
        if [[ $max_id == 0 ]]; then
            echo "There is no any remove_obs sgf files in the $sgf_path"
            exit 1
        fi

        for i in $(seq 1 $max_id)
        do
            touch -r $sgf_path/${i}_remove_obs.sgf $sgf_path/${i}.sgf # reset time stamp ({id}_remove_obs.sgf reference to {id}.sgf)
        done

        rm $sgf_path/[0-9]*_remove_obs\.sgf # remove all {id}_remove_obs.sgf
    fi

elif [[ $mode =~ ^(|-h|--help)$ ]]; then # help format "./handle_obs.sh -h"
    usage $mode
    exit 0

else
    echo "Unsupported mode: $mode"
    usage $mode
    exit 1
fi