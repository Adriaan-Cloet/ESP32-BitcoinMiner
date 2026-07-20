# Top-level ESP-IDF project file for the ESP32 build.
#
# This is a separate build system from the repo-root CMakeLists.txt: that one is
# the host build (gcc, the test bench, miner_desktop); this one is driven by
# idf.py and cross-compiles for the ESP32. The two never interfere because
# idf.py is run from inside this app_esp32/ directory.
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_solo_miner)