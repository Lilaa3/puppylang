cmake_minimum_required(VERSION 3.10.0)
project(puplang-wasm VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(puplang-wasm puplang-wasm.cpp)
target_link_libraries(puplang-wasm PRIVATE puplang)
set_target_properties(puplang-wasm PROPERTIES
    OUTPUT_NAME puplang
    SUFFIX .js)

target_link_options(puplang-wasm PRIVATE
    "SHELL:-O3"
    "SHELL:-sMODULARIZE=1"
    "SHELL:-sEXPORT_NAME=createPuplangModule"
    "SHELL:-sEXPORTED_FUNCTIONS=_puplang_encode,_puplang_decode,_puplang_last_error,_puplang_encode_file,_puplang_decode_file,_puplang_set_seed,_puplang_set_initial_howl_chance,_puplang_set_howl_decay,_puplang_set_free_extension_limit,_puplang_set_min_howl,_puplang_set_length_decay_rate,_puplang_set_howl_enabled,_puplang_set_uppercase_weight,_puplang_set_max_short_extension,_puplang_set_punct_as_char,_malloc,_free"
    "SHELL:-sEXPORTED_RUNTIME_METHODS=ccall,FS"
    "SHELL:-sALLOW_MEMORY_GROWTH=1"
    "SHELL:-sFILESYSTEM=1"
    "SHELL:-sENVIRONMENT=web,worker")
