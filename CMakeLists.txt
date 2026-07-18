cmake_minimum_required(VERSION 3.16)
project(esp32_solo_miner C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Warnings are errors from day one. On a project where bugs are silent, the
# compiler is the cheapest reviewer available.
add_compile_options(-Wall -Wextra -Wpedantic -Werror)

# The portable mining core. No platform dependencies live here, so these files
# compile unchanged for the host and for the ESP32.
add_library(miner_core
    core/sha256_ref.c
)
target_include_directories(miner_core PUBLIC core)

# Host-only test bench.
enable_testing()
add_executable(test_sha256 test/test_sha256.c)
target_link_libraries(test_sha256 PRIVATE miner_core)
add_test(NAME sha256 COMMAND test_sha256)