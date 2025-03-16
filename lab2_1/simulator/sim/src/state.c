#include <state.h>
#include <dbg.h>
#include <common.h>

uint8_t *mem = NULL;
CPU_state cpu = {};
int running = 1;

void exit_failure()
{
    if(mem) free(mem);
    exit(-1);
}

void exit_success()
{
    if(mem) free(mem);
    exit(0);
}

void init_state(const char *image)
{
    mem = (uint8_t *)malloc(MEM_SIZE);
    check_mem(mem);
    memset(mem, 0, MEM_SIZE);
    load_image(image);
    init_cpu();

}