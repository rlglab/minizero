file(GLOB SRCS *.cpp)

add_library(utils ${SRCS})
target_include_directories(utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(utils ${Boost_LIBRARIES})