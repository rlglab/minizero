file(GLOB SRCS *.cpp)

find_package(pybind11 REQUIRED)
pybind11_add_module(pybind_killallgo_learner ${SRCS} ../killallgo_configuration.cpp)
target_link_libraries(
    pybind_killallgo_learner
    PUBLIC
    config
    environment
    utils
)