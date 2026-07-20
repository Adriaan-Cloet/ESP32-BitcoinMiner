# ESP32 Bitcoin Miner

A from-scratch Bitcoin solo miner written in portable C, running both on a
desktop and on an ESP32, with the status shown over the serial console. It is a
learning project about how mining actually works end to end, not a machine meant
to make money.

> **Status: mining on both targets, with a real share accepted by the pool.**
> The same core code speaks Stratum v1, builds and verifies the coinbase,
> constructs the merkle root and block header with correct byte order, and grinds
> the nonce with a SHA-256 midstate optimisation. It runs on a laptop (with gdb
> and a test bench) and, unchanged, on a bare ESP32 with a FreeRTOS task split.
> The SHA-256 compression exists in two switchable versions, portable C and
> hand-written Xtensa assembly. A 0.96" SSD1306 OLED status display and an
> isolated VLAN are the remaining planned steps.

## Why this exists

This is a deliberate learning exercise. At the hash rate of a small device
against a network of hundreds of exahashes per second, the expected time to
actually find a block is on the order of tens of billions of years, so finding
one is not a goal and never will be. The point is to understand every step of
proof-of-work by implementing it, rather than calling a library that hides it.

If the goal were ever a lottery ticket that statistically means something, the
answer would be a purpose-built ASIC (for example a Bitaxe), not a
microcontroller. That would be a different project.

## What's inside

Everything in the pipeline is implemented by hand, in clear C:

- **SHA-256 and double-SHA-256**, a readable reference implementation used as the
  correctness oracle, plus an optimised version that reuses the header
  **midstate** so only the second block is recomputed per nonce. The block
  compression has two interchangeable implementations, portable C and
  hand-written Xtensa assembly.
- **Merkle root construction** from the coinbase hash and the branch the pool
  sends, folded one level at a time.
- **Block header assembly**, the 80 bytes you hash, resolving Bitcoin's mixed
  endianness in one place (version and time reversed, previous-hash byte-swapped
  within each 32-bit word, nonce little-endian).
- **The Stratum v1 protocol**: subscribe, authorize, notify, set_difficulty,
  suggest_difficulty and submit, parsed and serialized without touching a socket
  so every message is unit-testable.
- **Coinbase construction and payout verification**: the miner searches for its
  own scriptPubKey inside the coinbase the pool builds, the only real proof that
  the block reward is paid to you and not to someone else.
- **Difficulty-to-target conversion**, so a share is checked locally before it
  is submitted.
- **A line-oriented TCP client** over BSD sockets, which the ESP32's lwIP stack
  offers identically, so the network code ports unchanged.
- **An ESP32 firmware** that brings up WiFi and runs the miner across two
  FreeRTOS tasks: the nonce grind pinned to core 1, WiFi and Stratum on core 0,
  with found shares handed over a queue.

## Architecture

The design rule is that the mining core is platform-independent C. The same files
compile for the host (with gcc and a test bench) and for the ESP32 (with
ESP-IDF). Nothing in `core/` knows what a socket or a display is.

```
core/                         portable C, compiles for host and ESP32
  sha256_ref.c/h              reference SHA-256 / double-SHA-256 (the oracle)
  sha256_fast.c/h             midstate double-hash wrapper
  sha256_compress.c           SHA-256 block compression, portable C
  sha256_compress_xtensa.S    SHA-256 block compression, Xtensa assembly (ESP32)
  sha256_compress.h           shared interface for the two compressions
  target.c/h                  nBits expansion and target comparison
  merkle.c/h                  merkle root from the coinbase hash and branch
  header.c/h                  80-byte header assembly and byte order
  coinbase.c/h                coinbase assembly and payout verification
  difficulty.c/h              share difficulty to target
  stratum_proto.c/h           Stratum v1 parse/serialize (no sockets)
platform/
  net_posix.c/h               line-oriented TCP client over BSD sockets
app_desktop/main.c            the desktop mining loop
app_esp32/                    the ESP-IDF firmware
  main/main.c                 WiFi + FreeRTOS mining tasks
  main/selftest.c             on-device SHA-256 self-test firmware
  main/wifi.c/h               WiFi station bring-up
  main/Kconfig.projbuild      menuconfig option to pick C or assembly SHA-256
test/                         one executable per module, run via CTest
third_party/cJSON/            vendored JSON parser (MIT), used on both targets
```

## Results

The pool honours `mining.suggest_difficulty`, so the share difficulty drops to 1
and the miner produces shares within reach. On the desktop, one was accepted:

```
>> share found: job 2dd1423 nonce 0282c702 extranonce2 0000000000000000
share ACCEPTED (id 100)
```

### Optimisation, step by step

Two optimisations were applied on top of the naive miner: reusing the SHA-256
midstate (the header's first 64 bytes are constant per job, so their hash is done
once), and turning on compiler optimisation, which had been left at `-O0` on the
desktop and at `-Os` on the ESP32.

| Stage                     | Desktop (on battery) | ESP32 (240 MHz) |
|---------------------------|---------------------:|----------------:|
| Reference SHA-256         |            240 kH/s  |       3.9 kH/s  |
| + midstate                |            480 kH/s  |      16.7 kH/s  |
| + compiler optimisation   |          2 560 kH/s  |        20 kH/s  |

(Desktop figures are on battery; on mains in performance mode it goes higher
still. The `-O2` jump is large on the desktop because it had been building at
`-O0`, and small on the ESP32 because that was already `-Os`.)

```mermaid
xychart-beta
    title "Desktop hash rate by optimisation (kH/s, on battery)"
    x-axis ["reference", "+ midstate", "+ compiler -O2"]
    y-axis "kH/s" 0 --> 2700
    bar [240, 480, 2560]
```

```mermaid
xychart-beta
    title "ESP32 hash rate by optimisation (kH/s, 240 MHz)"
    x-axis ["reference", "+ midstate", "+ compiler"]
    y-axis "kH/s" 0 --> 22
    bar [3.9, 16.7, 20]
```

### C versus hand-written assembly

The SHA-256 block compression was then rewritten in Xtensa assembly. It is
correct, verified on the device itself against the reference over 20 000 random
headers, but measured on the ESP32 it is about 9% **slower** than the C the
compiler produces at `-O2`:

```mermaid
xychart-beta
    title "ESP32 SHA-256 throughput: C vs assembly (H/s, 240 MHz)"
    x-axis ["C at -O2", "Xtensa assembly"]
    y-axis "H/s" 0 --> 24000
    bar [22441, 20466]
```

That is the interesting result: a straightforward hand translation does not beat
a modern optimising compiler, which schedules the instructions better. Beating it
would take expert-level work (unrolling away the register rotation, avoiding the
`t1` spill, interleaving). Both versions are kept in the tree and selectable; the
C version is the default because it is faster.

## Getting started

Requirements for the desktop build: a C11 compiler, CMake 3.16 or newer, and Make
or Ninja. On Debian/Ubuntu/WSL:

```bash
sudo apt install build-essential cmake
```

Configure your pool and address. `config.h` is gitignored and never committed;
copy the template and fill in your own values:

```bash
cp config.example.h config.h
```

You need a Bitcoin address from your own wallet (a bech32 `bc1q...` address) and
its scriptPubKey (`0014` followed by the 20-byte key hash, which your wallet can
show). The device only ever knows this public address, never a private key. For
the ESP32 build the same file also holds the WiFi credentials.

Build and run the test bench, then the miner:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/miner_desktop
```

### On the ESP32

With [ESP-IDF](https://docs.espressif.com/projects/esp-idf/) installed and the
board connected, from the `app_esp32/` directory:

```bash
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

The miner joins WiFi, does the handshake, and grinds, logging the hash rate and
any accepted shares over the serial monitor. The SHA-256 compression defaults to
the portable C; to build with the Xtensa assembly instead, enable it under
`idf.py menuconfig` -> "Miner options". To flash the on-device self-test firmware
that checks the selected compression and reports throughput, build with
`idf.py -DMINER_SELFTEST=ON build`.

## How it works

For each job the pool sends, the miner assembles the coinbase
(`coinb1 + extranonce1 + extranonce2 + coinb2`), double-hashes it into the merkle
leaf, folds the branch into the merkle root, and assembles the 80-byte header
once. Because the header's first 64 bytes are then fixed, their SHA-256 is
computed a single time into a midstate; each nonce only varies the last four
bytes, so only the second block is hashed in the inner loop. A hit is sent with
`mining.submit`.

## Testing

Every module has its own test executable, and the whole bench is built with
`-Wall -Wextra -Wpedantic -Werror`: on a project where bugs are silent, the
compiler is the cheapest reviewer available.

Two tests carry most of the weight. The **block 125552 golden test** reconstructs
the canonical worked example from its fields in Stratum format and checks that the
assembled header and its double-SHA-256 match the published values byte for byte,
which pins down every endianness decision. The **SHA-256 differential test**
hashes tens of thousands of random headers through both the reference and the
optimised implementation and requires them to agree exactly. The same differential
check also runs as a firmware on the ESP32, which is how the hand-written assembly
is validated on real hardware.

## What this is not, and known limitations

- Not profitable, and not intended to be. See the odds above.
- The hand-written assembly is correct but slower than the compiler's C; making a
  hand version that actually wins would be a substantial further effort.
- A single device. Running several in parallel changes the odds from "never" to
  "never" and only adds complexity.
- No local Bitcoin node: work comes from a public solo pool over Stratum.
- The status display (a 0.96" SSD1306 OLED over I2C) and the isolated-VLAN network
  setup are still to come.

## Design decisions

A few choices are worth calling out, because making them deliberately is the
point:

- **C, not MicroPython.** An interpreter hides the very thing being learned, and
  the key optimisations live inside the hash function where Python cannot reach.
- **ESP-IDF, not Arduino.** IDF gives real C, real FreeRTOS and direct control,
  instead of a C++ layer that hides the internals.
- **One portable core.** `core/` has no platform dependencies, which is why the
  exact same files run under gdb on the desktop and on the ESP32 unchanged. The
  proof is that both targets mine from the same source.
- **Optimise the build before the code.** The biggest single win was simply
  turning on compiler optimisation; the desktop had been building unoptimised.
- **Assembly, measured not assumed.** The Xtensa version was written and then
  benchmarked against the compiler rather than presumed faster. It lost, which is
  the honest and more useful outcome. Both stay in the tree, switchable.
- **cJSON, vendored.** A small, readable MIT parser, compiled from the same
  vendored copy on both targets, so the wire format ports for free while the
  mining core stays fully hand-written.
- **A bech32 (P2WPKH) address.** Its scriptPubKey has a short, recognisable
  pattern, which makes the coinbase payout easy to verify by eye.

## License

MIT. See [LICENSE](LICENSE).