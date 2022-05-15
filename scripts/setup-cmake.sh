#!/bin/bash
set -e

usage() {
	echo "Usage: setup-cmake.sh [Debug|Release] [GO|GOMOKU|OTHELLO|KILLALLGO|TICTACTOE]"
	exit 1
}

if [ $# -eq 2 ]; then
	mode=$1
	if [ "${mode}" != "Debug" ] && [ "${mode}" != "Release" ]; then
		usage
	fi

	game_type=$2
	support_games=("GO" "GOMOKU" "OTHELLO" "KILLALLGO" "TICTACTOE")
	if [[ ! "${support_games[*]}" =~ "${game_type}" ]]; then
		usage
	fi

	echo "cmake . -DCMAKE_BUILD_TYPE=${mode} -DGAME_TYPE=${game_type}"
	cmake . -DCMAKE_BUILD_TYPE=${mode} -DGAME_TYPE=${game_type}
else
	usage
fi
