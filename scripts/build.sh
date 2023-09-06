#!/bin/bash
set -e

support_games=("atari" "chess" "go" "gomoku" "hex" "killallgo" "nogo" "othello" "puzzle2048" "rubiks" "tictactoe")

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
	[[ " ${support_games[*]} " == *" ${game_type} "* ]] || usage
	[ "${build_type}" == "Debug" ] || [ "${build_type}" == "Release" ] || usage

	# build
	echo "game type: ${game_type}"
	echo "build type: ${build_type}"
	if [ ! -f "build/${game_type}/Makefile" ]; then
		mkdir -p build/${game_type}
		cd build/${game_type}
		cmake ../../ -DCMAKE_BUILD_TYPE=${build_type} -DGAME_TYPE=${game_type^^}
	else
		cd build/${game_type}
	fi

	# create git info file
	git_hash=$(git log -1 --format=%H)
	git_short_hash=$(git describe --abbrev=6 --dirty --always --exclude '*')
	mkdir -p git_info
	git_info=$(echo -e "#pragma once\n\n#define GIT_HASH \"${git_hash}\"\n#define GIT_SHORT_HASH \"${git_short_hash}\"")
	if [ ! -f git_info/git_info.h ] || [ $(diff -q <(echo "${git_info}") <(cat git_info/git_info.h) | wc -l 2>/dev/null) -ne 0 ]; then
		echo "${git_info}" > git_info/git_info.h
	fi

	# make
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
