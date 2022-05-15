file(GLOB_RECURSE SRCS *.cpp)

add_library(environment ${SRCS})
target_include_directories(
    environment PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    base
    chess
    go
    gomoku
    killallgo
    othello
    tictactoe
)
target_link_libraries(
    environment
    config
    utils
)