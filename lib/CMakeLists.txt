cmake_minimum_required(VERSION 3.10.0)
project(puplang VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(puplang INTERFACE)

target_include_directories(puplang INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/thirdparty/utf8/source)

target_compile_features(puplang INTERFACE cxx_std_26)
