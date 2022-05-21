file(GLOB SRCS *.cpp)
add_executable(solver ${SRCS})
target_link_libraries(
    solver
    actor
    config
    console
    environment
    network
    utils
    ${Boost_LIBRARIES}
    ${TORCH_LIBRARIES}
)