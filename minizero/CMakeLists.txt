file(GLOB SRCS *.cpp learner/data_loader.cpp)

add_executable(minizero ${SRCS})
target_link_libraries(
    minizero
    actor
    config
    console
    environment
    network
    server
    utils
    ${TORCH_LIBRARIES}
)