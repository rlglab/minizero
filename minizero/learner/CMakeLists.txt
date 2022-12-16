file(GLOB SRCS *.cpp)

find_package(pybind11 REQUIRED)
pybind11_add_module(minizero_py ${SRCS})
target_link_libraries(
    minizero_py
    PUBLIC
    config
    environment
    utils
)

add_library(learner data_loader.cpp)
target_include_directories(
    learner PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(
    learner
    config
    environment
    utils
)