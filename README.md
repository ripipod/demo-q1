# Q1: LAST STAND

A native Quadra-1 / AstroEMU 2D survival-tower demo. The tower stays in the
middle of the arena; aim with the left analog stick and hold A to fire.
Enemies enter from the edges in increasingly large waves.

This project targets the guest machine implemented by
https://github.com/ripipod/Quadra-1-Emulator, not SDL2 or a desktop rendering API.
It uses AstroEMU's documented guest compiler subset, input MMIO, and Port-1 GPU
command queue.

> Compatibility note: this targets the current emulator-defined guest ABI. It
> is suitable for testing the current AstroEMU implementation, but is not a
> recovered original Quadra-1 SDK.

## Build

The source is in src/main.c. It uses the built-in integer C/C++ subset (no
system headers, pointers, arrays, floating point, or standard library).

From a command prompt with astroemu-cli.exe available:

    build.bat

Or run the compiler directly:

    astroemu-cli.exe --compile-folder .\src --out .\build

The output is build\program.qsb. In AstroEMU, use File -> Load & Run .qsb...
to load it. The active guest program runs one game frame per emulator frame
and keeps its game state in guest RAM.

## Controls

| Input | Action |
|---|---|
| Left stick | Aim the tower |
| Hold A | Fire |
| X (press) | Upgrade damage - 100 points |
| B (press) | Upgrade fire rate - 120 points |
| Y (press) | Repair tower by 25 HP - 100 points |
| Start (after defeat) | Restart |

Each kill awards 10 points. Wave sizes begin at 7, 9, 12, 23, then continue
growing. Enemy speed and health also increase as the run progresses. The game
uses fixed-capacity guest-RAM pools for enemies and bullets to keep allocation
predictable.

## Visual status indicators

- Top-left green/red bar: tower health.
- Top-right blue bar: enemies spawned in the current wave.
- Lower top-left amber bar: current score (scaled).
- Three colored pips at bottom-right: damage, fire-rate, and repair upgrades.

## Implementation notes

- 978x550 guest scanout.
- Input read from 0x10030000 and 0x10030004.
- GPU work is submitted as four-word packets using gpu_begin(),
  gpu_packet(), and gpu_end().
- GPU commands used: CLEAR, FILL_RECT, DRAW_LINE, and PRESENT.
- Enemy and projectile state is stored in guest RAM at 0x81100000 and
  0x81110000.
- Integer-only fixed-step movement; no host-side game logic or SDL dependency.
