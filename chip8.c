#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "raylib.h"

#define INSTURCTIONS_PER_SECOND 700
#define FPS 60
#define INSTRUCTIONS_PER_FRAME (INSTURCTIONS_PER_SECOND / FPS)

#define SCALING_FACTOR 15

typedef struct {
  uint8_t memory[4096];
  uint8_t registers[16];
  uint16_t register_I;
  uint16_t program_counter;
  bool display[64 * 32];
  uint8_t delay_timer;
  uint8_t sound_timer;
  uint16_t stack[16];
  uint16_t *stack_pointer;
  bool keypad[16];
} chip_8;

typedef struct {
  uint16_t text;
  uint16_t NNN;
  uint8_t NN;
  uint8_t N;
  uint8_t X;
  uint8_t Y;
} opcode;

bool chip_init(chip_8 *chip, char *path) {
  uint8_t font[] = {0xF0, 0x90, 0x90, 0x90, 0xF0,  // 0
                    0x20, 0x60, 0x20, 0x20, 0x70,  // 1
                    0xF0, 0x10, 0xF0, 0x80, 0xF0,  // 2
                    0xF0, 0x10, 0xF0, 0x10, 0xF0,  // 3
                    0x90, 0x90, 0xF0, 0x10, 0x10,  // 4
                    0xF0, 0x80, 0xF0, 0x10, 0xF0,  // 5
                    0xF0, 0x80, 0xF0, 0x90, 0xF0,  // 6
                    0xF0, 0x10, 0x20, 0x40, 0x40,  // 7
                    0xF0, 0x90, 0xF0, 0x90, 0xF0,  // 8
                    0xF0, 0x90, 0xF0, 0x10, 0xF0,  // 9
                    0xF0, 0x90, 0xF0, 0x90, 0x90,  // A
                    0xE0, 0x90, 0xE0, 0x90, 0xE0,  // B
                    0xF0, 0x80, 0x80, 0x80, 0xF0,  // C
                    0xE0, 0x90, 0x90, 0x90, 0xE0,  // D
                    0xF0, 0x80, 0xF0, 0x80, 0xF0,  // E
                    0xF0, 0x80, 0xF0, 0x80, 0x80}; // F

  memcpy(&chip->memory[0x050], font, sizeof(font));

  FILE *ROM = fopen(path, "rb");
  if (!ROM) {
    printf("ERROR: Can't open ROM file");
    return false;
  }

  int ROM_size;
  fseek(ROM, 0, SEEK_END);
  ROM_size = ftell(ROM);
  rewind(ROM);

  if (fread(&chip->memory[0x200], 1, ROM_size, ROM) != ROM_size) {
    printf("ERROR: Can't read ROM to memory");
    return false;
  }

  chip->program_counter = 0x200;
  chip->stack_pointer = &chip->stack[0];

  return true;
}

void emulate(chip_8 *chip) {
  opcode instruction;
  instruction.text = chip->memory[chip->program_counter] << 8 |
                     chip->memory[chip->program_counter + 1];
  instruction.NNN = instruction.text & 0x0FFF;
  instruction.NN = instruction.text & 0x00FF;
  instruction.N = instruction.text & 0x000F;
  instruction.X = (instruction.text & 0x0F00) >> 8;
  instruction.Y = (instruction.text & 0x00F0) >> 4;

  chip->program_counter += 2;

  switch (instruction.text & 0xF000) {
  case 0x0000:
    switch (instruction.text & 0x000F) {
    case 0x0000: // 00E0 - clear display
      memset(chip->display, false, sizeof(chip->display));
      break;
    }
    break;

  case 0x1000: // 1NNN - jump to NNN
    chip->program_counter = instruction.NNN;
    break;
  case 0x6000: // 6XNN - set VX to NN
    chip->registers[instruction.X] = instruction.NN;
    break;
  case 0x7000: // 7XNN - add NN to VX
    chip->registers[instruction.X] += instruction.NN;
    break;
  case 0xA000: // ANNN - set I to NNN
    chip->register_I = instruction.NNN;
    break;
  case 0xD000: // DXYN - draw sprite
    uint8_t x_pos = chip->registers[instruction.X] % 64;
    uint8_t y_pos = chip->registers[instruction.Y] % 32;
    uint8_t height = instruction.N;
    chip->registers[0xF] = 0;

    for (int row = 0; row < height; row++) {
      if (y_pos + row >= 32)
        break;

      uint8_t sprite = chip->memory[chip->register_I + row];
      for (int sprite_row = 0; sprite_row < 8; sprite_row++) {
        if (x_pos + sprite_row >= 64)
          break;

        uint8_t pixel = (sprite >> (7 - sprite_row)) & 1;

        if (pixel) {
          int index = (y_pos + row) * 64 + (x_pos + sprite_row);
          if (chip->display[index])
            chip->registers[0xF] = 1;

          chip->display[index] ^= 1;
        }
      }
    }
    break;
  }
}

void update_timer(chip_8 *chip) {
  if (chip->delay_timer > 0)
    chip->delay_timer--;

  if (chip->sound_timer > 0) {
    chip->sound_timer--;
    printf("BEEP!");
  }
}

void draw_screen(chip_8 *chip) {
  for (int i = 0; i < 64 * 32; i++)
    if (chip->display[i] == 1) {
      int x = (i % 64) * SCALING_FACTOR;
      int y = (i / 64) * SCALING_FACTOR;
      DrawRectangle(x, y, SCALING_FACTOR, SCALING_FACTOR, WHITE);
    }
}

void get_input(chip_8 *chip) {
  KeyboardKey key;
  key = GetKeyPressed();
  if (IsKeyPressed(key)) {
    switch (key) {
    case KEY_ONE:
      chip->keypad[0x1] = true;
      break;
    case KEY_TWO:
      chip->keypad[0x2] = true;
      break;
    case KEY_THREE:
      chip->keypad[0x3] = true;
      break;
    case KEY_FOUR:
      chip->keypad[0xC] = true;
      break;
    case KEY_Q:
      chip->keypad[0x4] = true;
      break;
    case KEY_W:
      chip->keypad[0x5] = true;
      break;
    case KEY_E:
      chip->keypad[0x6] = true;
      break;
    case KEY_R:
      chip->keypad[0xD] = true;
      break;
    case KEY_A:
      chip->keypad[0x7] = true;
      break;
    case KEY_S:
      chip->keypad[0x8] = true;
      break;
    case KEY_D:
      chip->keypad[0x9] = true;
      break;
    case KEY_F:
      chip->keypad[0xE] = true;
      break;
    case KEY_Z:
      chip->keypad[0xA] = true;
      break;
    case KEY_X:
      chip->keypad[0x0] = true;
      break;
    case KEY_C:
      chip->keypad[0xB] = true;
      break;
    case KEY_V:
      chip->keypad[0xF] = true;
      break;
    }
  } else if (IsKeyReleased(key)) {
    switch (key) {
    case KEY_ONE:
      chip->keypad[0x1] = false;
      break;
    case KEY_TWO:
      chip->keypad[0x2] = false;
      break;
    case KEY_THREE:
      chip->keypad[0x3] = false;
      break;
    case KEY_FOUR:
      chip->keypad[0xC] = false;
      break;
    case KEY_Q:
      chip->keypad[0x4] = false;
      break;
    case KEY_W:
      chip->keypad[0x5] = false;
      break;
    case KEY_E:
      chip->keypad[0x6] = false;
      break;
    case KEY_R:
      chip->keypad[0xD] = false;
      break;
    case KEY_A:
      chip->keypad[0x7] = false;
      break;
    case KEY_S:
      chip->keypad[0x8] = false;
      break;
    case KEY_D:
      chip->keypad[0x9] = false;
      break;
    case KEY_F:
      chip->keypad[0xE] = false;
      break;
    case KEY_Z:
      chip->keypad[0xA] = false;
      break;
    case KEY_X:
      chip->keypad[0x0] = false;
      break;
    case KEY_C:
      chip->keypad[0xB] = false;
      break;
    case KEY_V:
      chip->keypad[0xF] = false;
      break;
    }
  }
}

int main(int argc, char *argv[]) {
  chip_8 chip8;

  if (argc != 2) {
    printf("ERROR: correct usage CHIP-8 <path-to-ROM>");
    return 1;
  } else {
    if (!chip_init(&chip8, argv[1]))
      return 1;
  }

  InitWindow(64 * SCALING_FACTOR, 32 * SCALING_FACTOR, "CHIP-8");
  SetTargetFPS(FPS);

  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(BLACK);
    for (int i = 0; i < INSTRUCTIONS_PER_FRAME; i++) {
      emulate(&chip8);
    }

    draw_screen(&chip8);
    update_timer(&chip8);
    get_input(&chip8);

    EndDrawing();
  }
  CloseWindow();
}
