# 2048 Game (SDL3)

Project: classic 2048 game implemented in C using SDL3.
Play game https://qwertdim.github.io/2048-sdl3/

## Description

- Objective: slide tiles on a square grid (4x4), merging same-value tiles to reach 2048.
- Game board: 4 rows × 4 columns.
- Merge rules: when sliding in one direction, equal tiles merge into one (value doubled).
- Game over: no valid moves remain (board full and no adjacent equal tiles).

## Controls

- Arrow keys: move up/down/left/right.
- `R`: restart current game.
- `M`: mute/unmute sound.
- `Q` or closing the window: exit the game.

## Technologies

- Language: C
- Library: SDL3 (Simple DirectMedia Layer)
- Build system: CMake
- Platforms: Linux (tested), Windows (tested), Android(tested), Web(tested), possible macOS with SDL3.

## Licenses

- Game code: GPLv2 (GNU General Public License v2)
- Graphics, audio, text: CC0 (public domain)

## Build and Run

1. Install dependencies: SDL3, CMake, compiler (gcc/clang).
2. Build project:
```sh
mkdir -p build
cd build
cmake ..
cmake --build .
```
3. Run:
```sh
./game_2048
```

## Support

If something does not work, open an issue, submit a pull request, or contact the project author.

