file(GLOB_RECURSE SRCS *.cpp)

add_library(environment ${SRCS})
target_include_directories(
    environment PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    base
    amazons
    atari
    breakthrough
    clobber
    conhex
    connect6
    dotsandboxes
    go
    gomoku
    havannah
    hex
    killallgo
    linesofaction
    nogo
    othello
    rubiks
    santorini
    surakarta
    tictactoe
    stochastic
    stochastic/puzzle2048
    stochastic/tetrisblockpuzzle
)
target_link_libraries(
    environment
    config
    utils
    ale::ale-lib
    ${OpenCV_LIBS}
)
