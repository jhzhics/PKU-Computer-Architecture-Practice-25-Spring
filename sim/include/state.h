#ifndef __state_h__
#define __state_h__

#include <stdint.h>
#include <stddef.h>
#include <common.h>

extern uint8_t *mem;
extern CPU_state cpu;
extern int running;
extern uint64_t prog_brk;

void init_state(const char *image);
void exit_failure();
void exit_success();

#endif