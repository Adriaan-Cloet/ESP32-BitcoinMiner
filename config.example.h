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
 * Your own bech32 (bc1q...) payout address, taken from your wallet's Receive
 * tab. This is what the pool builds the coinbase output to, and it is sent as
 * the Stratum username. Some pools let you append a worker name as
 * "address.worker" for per-device stats.
 */
#define MINING_ADDRESS "bc1qexampleaddressreplacewithyourown00000"

/* Identifies this miner to the pool in mining.subscribe. Purely cosmetic. */
#define USER_AGENT     "esp32-solo-miner/0.1"

#endif /* CONFIG_H */