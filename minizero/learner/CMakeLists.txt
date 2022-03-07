file(GLOB SRCS *.cpp)

find_package(pybind11 REQUIRED)
pybind11_add_module(minizero_py ${SRCS})
target_link_libraries(
    minizero_py
    PUBLIC
    config
    environment
)