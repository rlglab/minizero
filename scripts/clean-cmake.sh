#!/bin/bash

echo "Clean up cmake files ... "
rm -rf detect_cuda_version.cc detect_cuda_compute_capabilities.cu detect_cuda_compute_capabilities.cpp CMakeCache.txt CMakeFiles Makefile cmake_install.cmake 
for cmake_dir in $(grep "add_subdirectory" CMakeLists.txt | awk -F "\(|\)" '{ print $2; }')
do
    rm -rf ${cmake_dir}/CMakeFiles ${cmake_dir}/Makefile ${cmake_dir}/cmake_install.cmake ${cmake_dir}/*.so
done

echo "Clean up complied files ... "
rm -rf Debug Release
