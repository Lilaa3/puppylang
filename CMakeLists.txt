cmake_minimum_required(VERSION 3.10.0)
project(puplang VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_CPP "Build the C++ implementation" ON)

if(BUILD_CPP)
    add_subdirectory(cpp-impl)
endif()
