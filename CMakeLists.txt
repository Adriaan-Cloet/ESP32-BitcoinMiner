cmake_minimum_required(VERSION 3.16)
project(esp32_solo_miner C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Warnings are errors from day one. On a project where bugs are silent, the
# compiler is the cheapest reviewer available.
add_compile_options(-Wall -Wextra -Wpedantic -Werror)

# Vendored cJSON (single file, MIT). On the ESP32 the same parser ships as an
# ESP-IDF component, so the Stratum code compiles unchanged for both targets.
# Third-party code is not held to our -Werror standard; we do not patch upstream,
# so warnings from it must not fail our build.
add_library(cjson third_party/cJSON/cJSON.c)
target_include_directories(cjson PUBLIC third_party/cJSON)
target_compile_options(cjson PRIVATE -w)

# The portable mining core. No platform dependencies live here, so these files
# compile unchanged for the host and for the ESP32.
add_library(miner_core
    core/sha256_ref.c
    core/sha256_fast.c
    core/target.c
    core/merkle.c
    core/header.c
    core/coinbase.c
    core/difficulty.c
    core/stratum_proto.c
)
target_include_directories(miner_core PUBLIC core)
# m: libm, for the powl/floorl used in difficulty_to_target. On the ESP32 the
# math functions come from newlib and no explicit link is needed.
target_link_libraries(miner_core PUBLIC cjson m)

# Platform layer: POSIX sockets. lwIP on the ESP32 also offers BSD sockets, so
# this file compiles there unchanged.
add_library(net_posix platform/net_posix.c)
target_include_directories(net_posix PUBLIC platform)

# Host-only test bench.
enable_testing()

add_executable(test_sha256 test/test_sha256.c)
target_link_libraries(test_sha256 PRIVATE miner_core)
add_test(NAME sha256 COMMAND test_sha256)

add_executable(test_sha256_fast test/test_sha256_fast.c)
target_link_libraries(test_sha256_fast PRIVATE miner_core)
add_test(NAME sha256_fast COMMAND test_sha256_fast)

add_executable(test_target test/test_target.c)
target_link_libraries(test_target PRIVATE miner_core)
add_test(NAME target COMMAND test_target)

add_executable(test_merkle test/test_merkle.c)
target_link_libraries(test_merkle PRIVATE miner_core)
add_test(NAME merkle COMMAND test_merkle)

add_executable(test_header test/test_header.c)
target_link_libraries(test_header PRIVATE miner_core)
add_test(NAME header COMMAND test_header)

add_executable(test_coinbase test/test_coinbase.c)
target_link_libraries(test_coinbase PRIVATE miner_core)
add_test(NAME coinbase COMMAND test_coinbase)

add_executable(test_difficulty test/test_difficulty.c)
target_link_libraries(test_difficulty PRIVATE miner_core)
add_test(NAME difficulty COMMAND test_difficulty)

add_executable(test_stratum_proto test/test_stratum_proto.c)
target_link_libraries(test_stratum_proto PRIVATE miner_core)
add_test(NAME stratum_proto COMMAND test_stratum_proto)

# Desktop app: runs the Stratum handshake against a real pool. It needs config.h
# (copied from config.example.h), which is gitignored. Only build it when that
# file exists, so a fresh clone still configures and runs the test bench.
if(EXISTS "${CMAKE_SOURCE_DIR}/config.h")
    add_executable(miner_desktop app_desktop/main.c)
    target_include_directories(miner_desktop PRIVATE "${CMAKE_SOURCE_DIR}")
    target_link_libraries(miner_desktop PRIVATE miner_core net_posix)
endif()