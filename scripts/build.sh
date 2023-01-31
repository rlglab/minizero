#!/bin/bash
set -e

support_games=("atari" "chess" "go" "gomoku" "hex" "killallgo" "othello" "tictactoe")

usage() {
	echo "Usage: build.sh games build_type"
	echo "  games: $(echo ${support_games[@]} | sed 's/ /, /g')"
	echo "  build_type: release(default), debug"
	exit 1
}

# add environment settings
git config core.hooksPath .githooks

# check game and build type
[ $# -ge 1 ] || usage
game_type=${1,,}
build_type=${2:-release}
build_type=$(echo ${build_type:0:1} | tr '[:lower:]' '[:upper:]')$(echo ${build_type:1} | tr '[:upper:]' '[:lower:]')
[[ "${support_games[*]}" =~ "${game_type}" ]] || usage
[ "${build_type}" == "Debug" ] || [ "${build_type}" == "Release" ] || usage

# build and make
echo "game type: ${game_type}"
echo "build type: ${build_type}"
[ ! -d "build" ] && mkdir build
[ ! -d "build/${game_type}" ] && mkdir build/${game_type}
cd build/${game_type}
cmake ../../ -DCMAKE_BUILD_TYPE=${build_type} -DGAME_TYPE=${game_type^^}
make -j$(nproc --all)