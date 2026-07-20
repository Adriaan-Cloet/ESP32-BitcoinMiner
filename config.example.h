/*
 * Configuration template. Copy this file to config.h and fill in your own
 * values:
 *
 *     cp config.example.h config.h
 *
 * config.h is listed in .gitignore and must never be committed. The device only
 * ever knows the public payout address below, never a private key or seed.
 */
#ifndef CONFIG_H
#define CONFIG_H

/* Solo pool endpoint. public-pool.io is a 0% fee, open-source solo pool. */
#define POOL_HOST      "public-pool.io"
#define POOL_PORT      "21496"

/*
 * WiFi credentials for the ESP32 build (ignored by the desktop build). Because
 * config.h is gitignored, these stay off GitHub. Keep the real password in your
 * password manager, not only here. Ideally use a separate, isolated WiFi network
 * for the miner (see the VLAN note).
 */
#define WIFI_SSID      "your-wifi-name"
#define WIFI_PASSWORD  "your-wifi-password"

/*
 * Your own bech32 (bc1q...) payout address, taken from your wallet's Receive
 * tab. This is what the pool builds the coinbase output to, and it is sent as
 * the Stratum username. Some pools let you append a worker name as
 * "address.worker" for per-device stats.
 */
#define MINING_ADDRESS "bc1qexampleaddressreplacewithyourown00000"

/*
 * The scriptPubKey your address decodes to, as hex. The miner searches for this
 * inside the pool's coinbase to prove the block reward is paid to you and not to
 * someone else. For a bech32 P2WPKH address it is "0014" + the 20-byte key hash;
 * your wallet (Sparrow: right-click the address, or the address details) shows
 * it. Leave the example value and the check simply reports "no".
 */
#define MINING_SCRIPTPUBKEY "0014000000000000000000000000000000000000000000"

/* Identifies this miner to the pool in mining.subscribe. Purely cosmetic. */
#define USER_AGENT     "esp32-solo-miner/0.1"

#endif /* CONFIG_H */