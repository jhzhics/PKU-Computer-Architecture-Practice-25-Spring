#ifndef SIM_H
#define SIM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define RANGE(st, ed)       (Area) { .start = (void *)(st), .end = (void *)(ed) }
#define sim_trap(code) asm volatile("mv a0, %0; ebreak" : :"r"(code))

typedef struct {
  void *start, *end;
} Area;


extern char _pmem_start;
#define PMEM_SIZE (128 * 1024 * 1024)
#define PMEM_END  ((uint64_t *)&_pmem_start + PMEM_SIZE)

#endif