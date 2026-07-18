# ESP32 Bitcoin Miner

A Bitcoin solo miner built from scratch for the ESP32, written in portable C on
ESP-IDF, with a 16x2 LCD as a status display.

> **Status: work in progress.** Phase 0 (the cryptographic core) is under
> construction. Nothing runs on hardware yet.

## Why this exists

To understand Bitcoin mining from the inside by building a working miner, rather
than by reading about it. Every part is written from first principles: the
SHA-256 implementation, the block header serialisation, the merkle root
construction, the Stratum client, and the LCD driver.

**This is not an attempt to make money.** At roughly 50 kH/s against a network of
around 870 EH/s, the expected time to find a block is on the order of hundreds of
billions of years. Efficiency is not a goal here; understanding is. If the goal
were ever a statistically meaningful lottery ticket, the answer would be a Bitaxe,
not an ESP32.

## What works today

- Reference SHA-256 implementation (FIPS 180-4), written for clarity as a test
  oracle for the optimised version that will follow.
- Test bench verifying:
  - NIST FIPS 180-4 vectors, including multi-block padding
  - The one-million-character message, which exercises block boundaries
  - **Bitcoin block 125552**, which proves the double SHA-256 and, critically,
    the byte order of the 80-byte block header

That last test is the foundation of the whole project. Byte-order bugs in Bitcoin
are silent: everything appears to run and you simply never find a valid hash.

## Architecture

The mining core is portable C that compiles for two targets: natively on a
development machine with a test bench, and unchanged on the ESP32. There is no
rewrite between the two, so there are no porting bugs.

This works because ESP-IDF uses lwIP, which provides BSD sockets. Even the
Stratum client is literally the same code on both.

```
core/         pure C, no platform dependencies
  sha256_ref  reference implementation, the test oracle
test/         host-only test bench
```

A key rule: the Stratum protocol layer takes a string and returns a struct. It
does not know that sockets exist, which makes the entire protocol testable with
hardcoded JSON and no network.

## Building and testing

Requires a C11 compiler and CMake 3.16 or newer.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/test_sha256
```

All tests should pass. The project builds with `-Wall -Wextra -Wpedantic -Werror`.

## Roadmap

| Phase | Scope | Status |
|---|---|---|
| 0 | SHA256d, merkle root, target arithmetic, test bench | in progress |
| 1 | Stratum v1 client, mining against a solo pool | planned |
| 2 | Port to ESP32, FreeRTOS task layout across both cores | planned |
| 3 | 16x2 LCD via I2C, hand-written HD44780 driver, temperature | planned |
| 4 | Isolated VLAN for the miner | planned |
| 5 | Xtensa assembly optimisation of the hash loop | optional |

## Security notes

- The device never holds a private key. It only sends a public Bitcoin address as
  the Stratum username, so there is nothing to steal from it. That is the entire
  security model.
- WiFi credentials and the payout address are read from a local `config.h` that
  is git-ignored. Copy `config.example.h` and fill in your own values.
- The miner is intended to run on an isolated network segment.

## Deliberately out of scope

Running a full node, mining across multiple boards, and building an enclosure.
Each is a different project.

## License

MIT. See [LICENSE](LICENSE).