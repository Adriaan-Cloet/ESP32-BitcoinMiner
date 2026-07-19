# ESP32 Bitcoin Miner

A from-scratch Bitcoin solo miner written in portable C, targeting an ESP32 with
a 16x2 LCD as a status display. It is a learning project about how mining
actually works end to end, not a machine meant to make money.

> **Status: phase 1 complete.** The miner runs on a desktop/laptop and mines
> against a live pool: it speaks Stratum v1, builds and verifies the coinbase,
> constructs the merkle root and block header with correct byte order, grinds
> the nonce, and submits shares that the pool accepts. The ESP32 firmware
> (phase 2) is next; the entire mining core is already written to compile
> unchanged for the microcontroller.

## Why this exists

This is a deliberate learning exercise. At the reference hash rate of a small
device against a network of hundreds of exahashes per second, the expected time
to actually find a block is on the order of tens of billions of years, so
finding one is not a goal and never will be. The point is to understand every
step of proof-of-work by implementing it, rather than calling a library that
hides it.

If the goal were ever a lottery ticket that statistically means something, the
answer would be a purpose-built ASIC (for example a Bitaxe), not a
microcontroller. That would be a different project.

## What's inside

Everything in the pipeline is implemented by hand, in clear C:

- **SHA-256 and double-SHA-256** (`sha256d`), a readable reference implementation
  used as the correctness oracle.
- **Merkle root construction** from the coinbase hash and the branch the pool
  sends, folded one level at a time.
- **Block header assembly**, the 80 bytes you hash, resolving Bitcoin's mixed
  endianness in one place (version and time reversed, previous-hash byte-swapped
  within each 32-bit word, nonce little-endian).
- **The Stratum v1 protocol**: subscribe, authorize, notify, set_difficulty,
  suggest_difficulty and submit, parsed and serialized without touching a socket
  so every message is unit-testable.
- **Coinbase construction and payout verification**: the miner searches for its
  own scriptPubKey inside the coinbase the pool builds, which is the only real
  proof that the block reward is paid to you and not to someone else.
- **Difficulty-to-target conversion**, so a share can be checked locally before
  it is submitted.
- **A line-oriented TCP client** over BSD sockets, which the ESP32's lwIP stack
  offers identically, so the network code ports unchanged.

## Architecture

The design rule is that the mining core is platform-independent C. The same
files compile for the host (with gcc and a test bench) and, in phase 2, for the
ESP32 (with ESP-IDF). Nothing in `core/` knows what a socket or a display is.

```
core/                 portable C, compiles for host and ESP32
  sha256_ref.c/h      SHA-256 and double-SHA-256 (the reference oracle)
  target.c/h          nBits expansion and target comparison (little-endian)
  merkle.c/h          merkle root from the coinbase hash and branch
  header.c/h          80-byte header assembly and byte order
  coinbase.c/h        coinbase assembly and payout (scriptPubKey) verification
  difficulty.c/h      share difficulty to target
  stratum_proto.c/h   Stratum v1 parse/serialize (no sockets)
platform/
  net_posix.c/h       line-oriented TCP client over BSD sockets
app_desktop/main.c    the desktop mining loop
test/                 one executable per module, run via CTest
third_party/cJSON/    vendored JSON parser (MIT); on the ESP32 it ships with ESP-IDF
```

## Getting started

Requirements: a C11 compiler, CMake 3.16 or newer, and Make or Ninja. On
Debian/Ubuntu/WSL:

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
show). The device only ever knows this public address, never a private key.

Build and run the test bench:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Then run the miner:

```bash
./build/miner_desktop
```

You should see the handshake, then `coinbase pays our address: yes` for each new
job, a periodic hash-rate line, and, once a share is found, whether the pool
accepted it.

## How it works

For each job the pool sends, the miner assembles the coinbase
(`coinb1 + extranonce1 + extranonce2 + coinb2`), double-hashes it into the
merkle leaf, folds the branch into the merkle root, and assembles the 80-byte
header once. It then varies only the last four bytes (the nonce), double-hashes
the header, and compares the result against the share target. A hit is sent with
`mining.submit`. When the nonce space is exhausted the miner advances the
extranonce2 and rebuilds.

## Testing

Every module has its own test executable, and the whole bench is built with
`-Wall -Wextra -Wpedantic -Werror`: on a project where bugs are silent, the
compiler is the cheapest reviewer available.

The most important test reconstructs **block 125552**, the canonical worked
example, from its fields in Stratum format and checks that the assembled header
and its double-SHA-256 match the published values byte for byte. If that passes,
every endianness decision is correct.

## Results

On a laptop the reference SHA-256 runs single-threaded at roughly 240 kH/s. The
pool honours `mining.suggest_difficulty`, dropping the share difficulty to 1, at
which point the miner produces shares the pool accepts. The reference hasher is
intentionally not optimised; making it fast is a later, optional step.

## What this is not, and known limitations

- Not profitable, and not intended to be. See the odds above.
- The `sha256d` used in the loop is the readable reference, not an optimised
  implementation. Hand-written assembly (roughly a 10x speedup) is a possible
  phase 5.
- A single device. Running several in parallel changes the odds from "never" to
  "never" and only adds complexity.
- No local Bitcoin node: work comes from a public solo pool over Stratum.
- The ESP32 firmware, the LCD driver, and the isolated-VLAN network setup are
  planned for phase 2 onward.

## Design decisions

A few choices are worth calling out, because making them deliberately is the
point:

- **C, not MicroPython.** An interpreter hides the very thing being learned, and
  the key optimisations live inside the hash function where Python cannot reach.
- **ESP-IDF, not Arduino.** IDF gives real C, real FreeRTOS and direct control,
  instead of a C++ layer that hides the internals.
- **cJSON for the wire format.** It ships with ESP-IDF, so the protocol code
  ports to the microcontroller for free, while the mining core stays fully
  hand-written.
- **A bech32 (P2WPKH) address.** Its scriptPubKey has a short, recognisable
  pattern, which makes the coinbase payout easy to verify by eye.

## License

MIT. See [LICENSE](LICENSE).
