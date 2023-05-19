cmake_minimum_required(VERSION 3.16)

project(minizero)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Torch REQUIRED)
find_package(Boost COMPONENTS system thread iostreams)
find_package(ale REQUIRED)
find_package(OpenCV REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -g -Wall -mpopcnt -O3 -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -Wall -Wno-unused-function -O0 -pthread")

# for git info
include_directories(${PROJECT_BINARY_DIR}/git_info)

add_subdirectory(minizero)
add_subdirectory(minizero/actor)
add_subdirectory(minizero/config)
add_subdirectory(minizero/console)
add_subdirectory(minizero/environment)
add_subdirectory(minizero/learner)
add_subdirectory(minizero/network)
add_subdirectory(minizero/utils)
add_subdirectory(minizero/zero)

string(TOLOWER "${PROJECT_NAME}_${GAME_TYPE}" EXE_FILE_NAME)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${EXE_FILE_NAME})
