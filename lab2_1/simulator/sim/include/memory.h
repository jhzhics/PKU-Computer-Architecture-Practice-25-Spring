#ifndef MEMORY_H
#define MEMORY_H

void load_image(char *filepath);
uint8_t* guest_to_host(uint64_t vaddr);
uint32_t inst_fetch(uint64_t pc);
uint64_t mem_read(uint64_t addr, int len);
void mem_write(uint64_t addr, int len, uint64_t data);

#endif
