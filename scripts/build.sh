#!/bin/bash
set -e

support_games=("atari" "chess" "go" "gomoku" "hex" "killallgo" "othello" "tictactoe")

usage() {
	echo "Usage: build.sh game_type build_type"
	echo "  game_type: $(echo ${support_games[@]} | sed 's/ /, /g')"
	echo "  build_type: release(default), debug"
	exit 1
}

build_game() {
	# check arguments is vaild
	game_type=${1,,}
	build_type=$2
	[[ "${support_games[*]}" =~ "${game_type}" ]] || usage
	[ "${build_type}" == "Debug" ] || [ "${build_type}" == "Release" ] || usage

	# build and make
	echo "game type: ${game_type}"
	echo "build type: ${build_type}"
	if [ ! -d "build/${game_type}" ]; then
		mkdir -p build/${game_type}
		cd build/${game_type}
		cmake ../../ -DCMAKE_BUILD_TYPE=${build_type} -DGAME_TYPE=${game_type^^}
	else
		cd build/${game_type}
	fi
	make -j$(nproc --all)
	cd ../..
}

# add environment settings
git config core.hooksPath .githooks

game_type=${1:-all}
build_type=${2:-release}
build_type=$(echo ${build_type:0:1} | tr '[:lower:]' '[:upper:]')$(echo ${build_type:1} | tr '[:upper:]' '[:lower:]')
[ "${game_type}" == "all" ] && [ ! -d "build" ] && usage

if [ "${game_type}" == "all" ]; then
	for game in build/*
	do
		build_game $(basename ${game}) ${build_type}
	done
else
	build_game ${game_type} ${build_type}
fi