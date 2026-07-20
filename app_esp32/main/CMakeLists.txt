# Component registration for the ESP32 miner.
#
# The core/ and platform/ sources are pulled in by relative path, so the exact
# same C that the host build and the test bench use is cross-compiled here.
#
# cJSON: we compile our own vendored copy (third_party/cJSON) here too, exactly
# like the host build, instead of relying on an ESP-IDF built-in "json"
# component. That component's name/location has changed across IDF versions, and
# vendoring keeps the build self-contained and version-proof. ESP-IDF does not
# apply -Werror to components, so cJSON's own warnings are harmless.
#
# SHA-256 compression: portable C (default) or the hand-written Xtensa assembly.
# Selected through menuconfig ("Miner options" -> "Use the hand-written Xtensa
# assembly..."), stored in the gitignored sdkconfig. The default is C, so a plain
# build of the miner needs no choice; both sources live in core/.
if(CONFIG_MINER_USE_XTENSA_SHA256)
    set(SHA256_COMPRESS "../../core/sha256_compress_xtensa.S")
else()
    set(SHA256_COMPRESS "../../core/sha256_compress.c")
endif()

# Dev-only: build the on-device SHA-256 self-test firmware instead of the miner.
# It checks the selected compression against the reference over many random
# headers and prints a throughput number. Pass -DMINER_SELFTEST=ON to idf.py.
option(MINER_SELFTEST "Build the SHA-256 self-test instead of the miner" OFF)
if(MINER_SELFTEST)
    set(APP_SRC "selftest.c")
else()
    set(APP_SRC "main.c")
endif()

idf_component_register(
    SRCS
        "${APP_SRC}"
        "wifi.c"
        "../../core/sha256_ref.c"
        "../../core/sha256_fast.c"
        "${SHA256_COMPRESS}"
        "../../core/target.c"
        "../../core/merkle.c"
        "../../core/header.c"
        "../../core/coinbase.c"
        "../../core/difficulty.c"
        "../../core/stratum_proto.c"
        "../../platform/net_posix.c"
        "../../third_party/cJSON/cJSON.c"
    INCLUDE_DIRS
        "."
        "../../core"
        "../../platform"
        "../../third_party/cJSON"
        "../.."          # the repo root, for config.h
    REQUIRES
        esp_wifi
        esp_event
        esp_netif
        nvs_flash
        esp_timer
        lwip
)