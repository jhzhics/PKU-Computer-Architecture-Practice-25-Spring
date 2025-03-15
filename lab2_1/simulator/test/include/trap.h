#ifndef TRAP_H
#define TRAP_H

#define LENGTH(arr)         (sizeof(arr) / sizeof((arr)[0]))

void halt(int code);
__attribute__((noinline))
void check(int cond) {
  if (!cond) halt(1);
}

#endif