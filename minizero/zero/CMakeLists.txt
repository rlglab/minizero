file(GLOB SRCS *.cpp)

add_library(zero ${SRCS})
target_include_directories(
    zero PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(
    zero
    config
    utils
    ${Boost_LIBRARIES}
)