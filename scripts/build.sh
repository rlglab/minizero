#!/bin/bash
set -e

env_cmakelists="$(dirname $(readlink -f "$0"))/../minizero/environment/CMakeLists.txt"
support_games=($(awk '/target_include_directories/,/\)/' ${env_cmakelists} | sed 's|/|\n|g' | grep -v -E 'target|environment|PUBLIC|CMAKE_CURRENT_SOURCE_DIR|base|stochastic|)'))

usage() {
	echo "Usage: $0 GAME_TYPE BUILD_TYPE"
	echo ""
	echo "Required arguments:"
	echo "  GAME_TYPE: $(echo ${support_games[@]} | sed 's/ /, /g')"
	echo "  BUILD_TYPE: release(default), debug"
	exit 1
}

build_game() {
	# check arguments is vaild
	game_type=${1,,}
	build_type=$2
	[[ " ${support_games[*]} " == *" ${game_type} "* ]] || usage
	[ "${build_type}" == "Debug" ] || [ "${build_type}" == "Release" ] || usage

	# check whether the build type and cache are consistent
	if [ -f "build/${game_type}/CMakeCache.txt" ]; then
		cache_build_type=$(grep -oP "CMAKE_BUILD_TYPE:STRING=\K\w+" build/${game_type}/CMakeCache.txt)
		if [ "${cache_build_type}" != "${build_type}" ]; then
			rm -rf build/${game_type}
		fi
	fi

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
