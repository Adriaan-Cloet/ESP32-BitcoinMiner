/*
 * Minimal WiFi station bring-up for the ESP32 miner.
 *
 * The miner only needs one outgoing TCP connection, so all this does is join a
 * network and block until an IP arrives. Credentials come from config.h (which
 * is gitignored) and never live in the repo.
 */
#ifndef WIFI_H
#define WIFI_H

#include <stdbool.h>

/*
 * Connect to the given network and block until connected or the retries run
 * out. Returns true once an IP address has been assigned.
 */
bool wifi_connect(const char *ssid, const char *password);

#endif /* WIFI_H */