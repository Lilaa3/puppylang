cmake_minimum_required(VERSION 3.10.0)
project(puplang-wasm VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Built via the Emscripten toolchain (emcmake cmake ...).
add_executable(puplang-wasm puplang-wasm.cpp)

target_link_libraries(puplang-wasm PRIVATE puplang)

# #embed (C++26) still trips clang's extension warning under emcc.
target_compile_options(puplang-wasm PRIVATE -Wno-c23-extensions)

set_target_properties(puplang-wasm PROPERTIES
    OUTPUT_NAME puplang
    SUFFIX .js)

target_link_options(puplang-wasm PRIVATE
    "SHELL:-O3"
    "SHELL:-sMODULARIZE=1"
    "SHELL:-sEXPORT_NAME=createPuplangModule"
    "SHELL:-sEXPORTED_FUNCTIONS=_puplang_encode,_puplang_decode,_puplang_last_error,_malloc,_free"
    "SHELL:-sEXPORTED_RUNTIME_METHODS=ccall"
    "SHELL:-sALLOW_MEMORY_GROWTH=1"
    "SHELL:-sFILESYSTEM=0"
    "SHELL:-sENVIRONMENT=web,node"
    "SHELL:-sSTRICT=1")
