cmake_minimum_required(VERSION 3.16)
project(esp32_solo_miner C)

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
    core/target.c
    core/stratum_proto.c
)
target_include_directories(miner_core PUBLIC core)
target_link_libraries(miner_core PUBLIC cjson)

# Host-only test bench.
enable_testing()

add_executable(test_sha256 test/test_sha256.c)
target_link_libraries(test_sha256 PRIVATE miner_core)
add_test(NAME sha256 COMMAND test_sha256)

add_executable(test_target test/test_target.c)
target_link_libraries(test_target PRIVATE miner_core)
add_test(NAME target COMMAND test_target)

add_executable(test_stratum_proto test/test_stratum_proto.c)
target_link_libraries(test_stratum_proto PRIVATE miner_core)
add_test(NAME stratum_proto COMMAND test_stratum_proto)