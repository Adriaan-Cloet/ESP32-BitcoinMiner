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
idf_component_register(
    SRCS
        "main.c"
        "wifi.c"
        "../../core/sha256_ref.c"
        "../../core/sha256_fast.c"
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