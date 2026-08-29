cmake_minimum_required(VERSION 3.10.0)
project(puplang-cli VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(EMBED_LICENSES "Embed third-party license texts into the CLI" ON)
set(CREDITS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/credits.txt")

set(PUPLANG_CLI_SOURCES
    main.cpp
)
if(EMBED_LICENSES)
    set(PUPLANG_EMBED_LINE "\"${PUPLANG_LICENSE_FILE}\"")
    set(ARGPARSE_EMBED_LINE "\"${ARGPARSE_LICENSE_FILE}\"")
    set(UTFCPP_EMBED_LINE "\"${UTFCPP_LICENSE_FILE}\"")

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/licenses.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/licenses.cpp
        @ONLY)
    list(APPEND PUPLANG_CLI_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/licenses.cpp)
endif()

add_executable(puplang-cli ${PUPLANG_CLI_SOURCES})
target_link_libraries(puplang-cli PRIVATE puplang)

target_include_directories(puplang-cli PRIVATE
    ${CMAKE_SOURCE_DIR}/thirdparty/argparse/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR})

if(EMBED_LICENSES)
    target_compile_definitions(puplang-cli PRIVATE PUPLANG_EMBED_LICENSES)
endif()

set_target_properties(puplang-cli PROPERTIES OUTPUT_NAME puplang-cli)
