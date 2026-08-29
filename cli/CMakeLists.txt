cmake_minimum_required(VERSION 3.10.0)
project(puplang-cli VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(puplang-cli main.cpp)

target_link_libraries(puplang-cli PRIVATE puplang)

target_include_directories(puplang-cli PRIVATE
    ${CMAKE_SOURCE_DIR}/thirdparty/argparse/include)

set_target_properties(puplang-cli PROPERTIES OUTPUT_NAME puplang-cli)
