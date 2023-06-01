file(GLOB_RECURSE SRCS *.cpp)

add_library(network ${SRCS})
target_include_directories(
    network PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(
    network
    utils
    ${TORCH_LIBRARIES}
)