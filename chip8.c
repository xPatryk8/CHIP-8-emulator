#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"

#define INSTURCTIONS_PER_SECOND 700

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

  memset(&chip->memory[0], 0, sizeof(chip->memory));
  memcpy(&chip->memory[0x050], font, sizeof(font));
  memset(&chip->keypad[0], false, sizeof(chip->keypad));
  memset(&chip->display, false, sizeof(chip->display));

  chip->program_counter = 0x200;
  chip->register_I = 0;
  chip->stack_pointer = &chip->stack[0];
  chip->delay_timer = 0;
  chip->sound_timer = 0;

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

  fclose(ROM);

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
    switch (instruction.text & 0x00FF) {
    case 0x00E0: // 00E0 - clear display
      memset(chip->display, false, sizeof(chip->display));
      break;
    case 0x00EE: // 00EE - return subroutine
      chip->stack_pointer--;
      chip->program_counter = *chip->stack_pointer;
      break;
    }
    break;
  case 0x1000: // 1NNN - jump to NNN
    chip->program_counter = instruction.NNN;
    break;
  case 0x2000: // 2NNN - call subroutine at NNN
    if (chip->stack_pointer > &chip->stack[16]) {
      printf("ERROR: Stack overflow!");
      return;
    }
    *chip->stack_pointer = chip->program_counter;
    chip->stack_pointer++;
    chip->program_counter = instruction.NNN;
    break;
  case 0x3000: // 3XNN - skip instruction if VX = NN
    if (chip->registers[instruction.X] == instruction.NN)
      chip->program_counter += 2;
    break;
  case 0x4000: // 4XNN - skip if VX != NN
    if (chip->registers[instruction.X] != instruction.NN)
      chip->program_counter += 2;
    break;
  case 0x5000: // 5XY0 - skip if VX = VY
    if (chip->registers[instruction.X] == chip->registers[instruction.Y])
      chip->program_counter += 2;
    break;
  case 0x6000: // 6XNN - set VX to NN
    chip->registers[instruction.X] = instruction.NN;
    break;
  case 0x7000: // 7XNN - add NN to VX
    chip->registers[instruction.X] += instruction.NN;
    break;
  case 0x8000:
    switch (instruction.text & 0x000F) {
    case 0x0000: // 8XY0 - set VX to VY
      chip->registers[instruction.X] = chip->registers[instruction.Y];
      break;
    case 0x0001: // 8XY1 - set VX to VX | VY
      chip->registers[instruction.X] |= chip->registers[instruction.Y];
      break;
    case 0x0002: // 8XY2 - set VX to VX & VY
      chip->registers[instruction.X] &= chip->registers[instruction.Y];
      break;
    case 0x0003: // 8XY3 - set VX to VX ^ VY
      chip->registers[instruction.X] ^= chip->registers[instruction.Y];
      break;
    case 0x0004: // 8XY4 - add VY to VX, VF is set to 1 if there's overflo
      uint16_t sum =
          chip->registers[instruction.X] + chip->registers[instruction.Y];
      if (sum > 0xFF)
        chip->registers[0xF] = 1;
      else
        chip->registers[0xF] = 0;

      chip->registers[instruction.X] = (uint8_t)sum;
      break;
    case 0x0005: // 8XY5 - subtract VY from VX, set VF if underflow
      if (chip->registers[instruction.X] >= chip->registers[instruction.Y])
        chip->registers[0xF] = 1;
      else
        chip->registers[0xF] = 0;

      chip->registers[instruction.X] -= chip->registers[instruction.Y];
      break;
    case 0x0006: // 8XY6 - shifts VX to right by 1, stores least significant bit
                 // in VF
      chip->registers[0xF] = chip->registers[instruction.X] & 0x01;
      chip->registers[instruction.X] >>= 1;
      break;
    case 0x0007: // 8XY7 - set VX TO VY - VX, set VF to 0 if ubderflow
      if (chip->registers[instruction.Y] >= chip->registers[instruction.X])
        chip->registers[0xF] = 1;
      else
        chip->registers[0xF] = 0;

      chip->registers[instruction.X] =
          chip->registers[instruction.Y] - chip->registers[instruction.X];
      break;
    case 0x000E: // 8XYE - shifts VX to left by 1, sets VF to 1 if most
                 // significant bit was set
      chip->registers[0xF] = chip->registers[instruction.X] >> 7;
      chip->registers[instruction.X] <<= 1;
      break;
    }
    break;
  case 0x9000: // 9XY0 - skip if VX != VY
    if (chip->registers[instruction.X] != chip->registers[instruction.Y])
      chip->program_counter += 2;
    break;
  case 0xA000: // ANNN - set I to NNN
    chip->register_I = instruction.NNN;
    break;
  case 0xB000: // BNNN - jump to NNN + V0;
    chip->program_counter = instruction.NNN + chip->registers[0];
    break;
  case 0xC000: // CXNN - set VX to and on a random number
    chip->registers[instruction.X] = (rand() % 256) & instruction.NN;
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
  case 0xE000:
    switch (instruction.text & 0x00FF) {
    case 0x009E: // EX9E - skip if key in VX is pressed
      if (chip->keypad[chip->registers[instruction.X] & 0x0F] == 1)
        chip->program_counter += 2;
      break;
    case 0x00A1: // EXA1 - skip if key in VX is not pressed
      if (chip->keypad[chip->registers[instruction.X] & 0x0F] == 0)
        chip->program_counter += 2;
      break;
    }
    break;
  case 0xF000:
    switch (instruction.text & 0x00FF) {
    case 0x0007: // FX07 - set VX to delay timer
      chip->registers[instruction.X] = chip->delay_timer;
      break;
    case 0x000A: // FX0A - wait for key and sotre in VX
      bool key_pressed = false;
      for (uint8_t i = 0; i < 16; i++)
        if (chip->keypad[i]) {
          key_pressed = true;
          chip->registers[instruction.X] = i;
          break;
        }
      if (!key_pressed)
        chip->program_counter -= 2;
      break;
    case 0x0015: // FX15 - set delay_timer to VX
      chip->delay_timer = chip->registers[instruction.X];
      break;
    case 0x0018: // FX18 - set sound timer to VX
      chip->sound_timer = chip->registers[instruction.X];
      break;
    case 0x001E: // FX1E - add VX to I
      chip->register_I += chip->registers[instruction.X];
      break;
    case 0x0029: // FX29 - set I to location of sprite for charachter in VX
      chip->register_I = 0x50 + (chip->registers[instruction.X] & 0x0F) * 5;
      break;
    case 0x0033: // FX33 - stores binary-coded decimal representation of VX
      chip->memory[chip->register_I] = chip->registers[instruction.X] / 100;
      chip->memory[chip->register_I + 1] =
          (chip->registers[instruction.X] / 10) % 10;
      chip->memory[chip->register_I + 2] = chip->registers[instruction.X] % 10;
      break;
    case 0x0055: // FX55 - store all registers in memory
      for (uint8_t i = 0; i <= instruction.X; i++)
        chip->memory[chip->register_I + i] = chip->registers[i];
      break;
    case 0x0065: // FX65 - load values from memory to registers
      for (uint8_t i = 0; i <= instruction.X; i++)
        chip->registers[i] = chip->memory[chip->register_I + i];
      break;
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
  KeyboardKey keys[16] = {KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_Q, KEY_W,
                          KEY_E,   KEY_R,   KEY_A,     KEY_S,    KEY_D, KEY_F,
                          KEY_Z,   KEY_X,   KEY_C,     KEY_V};

  for (int i = 0; i < 16; i++) {
    if (IsKeyPressed(keys[i]))
      chip->keypad[i] = true;
    else if (IsKeyReleased(keys[i]))
      chip->keypad[i] = false;
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

  uint32_t time = 1000 / INSTURCTIONS_PER_SECOND;
  uint32_t hertz = 1000 / 60;

  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(BLACK);
    get_input(&chip8);

    for (uint32_t i = 0; i < time; i++)
      emulate(&chip8);

    for (uint32_t i = 0; i < hertz; i++) {
      draw_screen(&chip8);
      update_timer(&chip8);
    }

    EndDrawing();
  }
  CloseWindow();
}
