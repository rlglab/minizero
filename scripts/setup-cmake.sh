#!/bin/bash
set -e

usage() {
	echo "Usage: setup-cmake.sh [Debug|Release] [GO|TIETACTOE]"
	exit 1
}

if [ $# -eq 2 ]; then
	mode=$1
	if [ "${mode}" != "Debug" ] && [ "${mode}" != "Release" ]; then
		usage
	fi

	game_type=$2
	if [ "${game_type}" != "GO" ] && [ "${game_type}" != "TIETACTOE" ]; then
		usage
	fi

	echo "cmake . -DCMAKE_BUILD_TYPE=${mode} -DGAME_TYPE=${game_type}"
	cmake . -DCMAKE_BUILD_TYPE=${mode} -DGAME_TYPE=${game_type}
else
	usage
fi
