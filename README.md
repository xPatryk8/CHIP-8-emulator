### CHIP-8 Emulator

CHIP-8 emulator written in C using Raylib.

## Building

### Requriments

- C compiler
- Cmake 3.14 or higher

Raylib is automatically downloaded and built by CMake.

### Compilation

    git clone https://github.com/xPatryk8/CHIP-8-emulator
    cd CHIP-8-emulator
    cmake -B <path-to-build>
    cmake --build <path-to-build> --config Release

## Usage

    chip-8 <path-to-ROM

## Controls

``` bash
  CHIP-8  | Keyboard
  1 2 3 C | 1 2 3 4
  4 5 6 D | Q W E R
  7 8 9 E | A S D F
  A 0 B F | Z X C V
```

**Esc:** exit

## Resources

- [CHIP-8 Wikipedia](https://en.wikipedia.org/wiki/CHIP-8#Opcode_table);
- [Guide to making CHIP-8 emulator](https://tobiasvl.github.io/blog/write-a-chip-8-emulator/)
- [CHIP-8 ROMs](https://github.com/kripod/chip8-roms)
- [CHIP-8 ROM pack](https://www.zophar.net/pdroms/chip8/chip-8-games-pack.html)

## ToDo

- Add sound
