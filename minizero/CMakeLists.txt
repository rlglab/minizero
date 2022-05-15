file(GLOB SRCS *.cpp)

add_executable(minizero ${SRCS})
target_link_libraries(
    minizero
    actor
    config
    console
    environment
    network
    server
    #solver
    utils
    ${TORCH_LIBRARIES}
)