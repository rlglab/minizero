file(GLOB SRCS *.cpp)

add_library(config ${SRCS})
target_include_directories(config PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(config)
target_compile_definitions(config PUBLIC ${GAME_TYPE})