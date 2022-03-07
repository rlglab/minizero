file(GLOB SRCS *.cpp)

add_library(server ${SRCS})
target_include_directories(
    server PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(
    server
    config
    utils
    ${Boost_LIBRARIES}
)