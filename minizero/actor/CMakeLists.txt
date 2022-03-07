file(GLOB_RECURSE SRCS *.cpp)

add_library(actor ${SRCS})
target_include_directories(actor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
    actor
    config
    environment
    network
    utils
    ${Boost_LIBRARIES}
    ${TORCH_LIBRARIES}
)