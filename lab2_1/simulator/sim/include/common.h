#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <dbg.h>

#define MEM_SIZE 0x8000000
#define MEM_BASE 0x80000000

typedef struct {
    uint64_t reg[32];
    uint64_t pc;
} CPU_state;

typedef struct {
  uint64_t pc;
  uint64_t snpc; // static next pc
  uint64_t dnpc; // dynamic next pc
  uint32_t inst;
} Decode;

#endif