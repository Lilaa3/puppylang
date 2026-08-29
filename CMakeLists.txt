cmake_minimum_required(VERSION 3.10.0)
project(puplang VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_CLI "Build the command-line tool" ON)
option(BUILD_WASM "Build the WebAssembly module (requires Emscripten)" OFF)

# License file locations
set(PUPLANG_LICENSE_FILE "${CMAKE_SOURCE_DIR}/LICENSE")
set(ARGPARSE_LICENSE_FILE "${CMAKE_SOURCE_DIR}/thirdparty/argparse/LICENSE")
set(UTFCPP_LICENSE_FILE "${CMAKE_SOURCE_DIR}/thirdparty/utf8/LICENSE")

add_subdirectory(lib)

if(BUILD_CLI)
    add_subdirectory(cli)
endif()

if(BUILD_WASM)
    add_subdirectory(web)
endif()
