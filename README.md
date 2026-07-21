
# atto-miner

An ultra-lightweight, zero-dependency Duino-Coin (DUCO-S1) miner written in C.

---

## Features

* **Zero Dependencies:** Compiles directly against standard system libraries.
* **Dual Execution Modes:**
  * `normal`: Interactive CLI prompt for credentials at runtime.
  * `minimal`: Static, non-logging background daemon with credentials baked in at compile time.
* **Portable HAL Architecture:** Native platform backends for Linux and Windows (`hal_linux.c`, `hal_windows.c`).
* **Multi-threaded:** Parallel execution using native OS threads.

---

## Prerequisites

* GCC or Clang
* `x86_64-w64-mingw32-gcc` (for Windows cross-compilation)
* `musl-gcc` (optional, for Linux static builds)

---

## Compilation

Build targets are configured via `make` flags:

### Standard Build (`MODE=normal`)

```bash
# Linux (Default)
make

# Windows
make ARCH=windows

```

### Static / Minimal Build (`MODE=minimal`)

```bash
# Linux static build
make MODE=minimal USERNAME=user MINING_KEY=key RIG_ID=rig1 THREADS=2

# Windows static build
make ARCH=windows MODE=minimal USERNAME=user MINING_KEY=key RIG_ID=rig1 THREADS=4

```

---

## Usage

### Interactive Mode

```bash
# Launch interactive prompts
./atto-miner-linux-x64

# Creates run.sh with your credentials
./configure_linux.sh

```

### Minimal Mode

Runs automatically using flags provided during build time:

```bash
./atto-miner-linux-static-x64

```

---

## Makefile Options

| Flag | Options | Default | Description |
| --- | --- | --- | --- |
| `ARCH` | `linux`, `windows` | `linux` | Target operating system |
| `MODE` | `normal`, `minimal` | `normal` | Interactive vs static/silent build |
| `USERNAME` | `string` | `username` | DUCO account username (`minimal` mode) |
| `MINING_KEY` | `string` | `password` | DUCO mining key (`minimal` mode) |
| `RIG_ID` | `string` | `atto-static` | Identifier reported to pool |
| `THREADS` | `number` | `1` | Total worker threads |

---

## Project Structure

```text
├── main.c        # Core logic, job processing, entry point
├── sha1.c        # SHA-1 implementation
├── sha1.h        # SHA-1 header
└── hal/          # Hardware Abstraction Layer
    ├── hal.h     # Network and thread abstraction API
    ├── hal_linux.c
    └── hal_windows.c

```

---

## License

Distributed under the **MIT License**. Inspired by `d-cpuminer`.

