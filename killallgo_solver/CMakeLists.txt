file(GLOB SRCS *.cpp)
add_executable(killallgo_solver ${SRCS})
target_link_libraries(
    killallgo_solver
    actor
    config
    console
    environment
    network
    server
    utils
    ${Boost_LIBRARIES}
    ${TORCH_LIBRARIES}
)