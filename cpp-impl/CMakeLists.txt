cmake_minimum_required(VERSION 3.10.0)
project(puplang-cpp VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(puplang-cpp main.cpp)

target_include_directories(puplang-cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/utf8/source)
target_include_directories(puplang-cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/argparse/include)

