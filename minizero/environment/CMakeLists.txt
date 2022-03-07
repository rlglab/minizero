file(GLOB_RECURSE SRCS *.cpp)

add_library(environment ${SRCS})
target_include_directories(
    environment PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    base
    go
    tietactoe
)
target_link_libraries(
    environment
    utils
)
target_compile_definitions(environment PUBLIC ${GAME_TYPE})