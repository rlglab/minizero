file(GLOB_RECURSE SRCS *.cpp)

add_library(solver ${SRCS})
target_include_directories(
    solver PUBLIC
    killallgo
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(
    solver
    actor
    config
    environment
    network
    utils
    ${Boost_LIBRARIES}
    ${TORCH_LIBRARIES}
)