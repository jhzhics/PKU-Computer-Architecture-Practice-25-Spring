#include <common.h>
#include <memory.h>
#include <cpu.h>

uint8_t *mem = NULL;

int main(int argc, char *argv[]){
    mem = (uint8_t *)malloc(MEM_SIZE);
    check_mem(mem);
    memset(mem, 0, MEM_SIZE);
    load_image(argv[1]);
    init_cpu();
    cpu_exec();
    free(mem);
    return 0;

error:
    if(mem) free(mem);
    return -1;
}
