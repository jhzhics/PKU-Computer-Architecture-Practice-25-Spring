#include <common.h>
#include <mem.h>
#include <state.h>
#include <assert.h>

struct __Mem_trace mem_trace;

uint8_t* guest_to_host(uint64_t addr) {return mem + addr - MEM_BASE;}

static inline uint64_t host_read(void *addr, int len){
    switch(len){
        case 1: return *(uint8_t  *)addr;
        case 2: return *(uint16_t *)addr;
	    case 4: return *(uint32_t *)addr;
        case 8: return *(uint64_t *)addr;
        default: sentinel("Invalid len for read.");
    }
error:
    return 0;
}

static inline void host_write(void *addr, int len, uint64_t data){
    switch(len){
        case 1: *(uint8_t  *)addr = data; return;
        case 2: *(uint16_t *)addr = data; return;
        case 4: *(uint32_t *)addr = data; return;
        case 8: *(uint64_t *)addr = data; return;
        default: sentinel("Invalid len for write.");
    }
    
error:
    return;
}

uint32_t inst_fetch(uint64_t pc){
    check(pc, "PC is zero.");
    return (*(uint32_t *)guest_to_host(pc));
error:
    return 0;
}

uint64_t mem_read(uint64_t addr, int len){
    check(addr - MEM_BASE < MEM_SIZE, "Read addr %016lx out of bound. PC %08lx", addr, cpu.pc);
    uint64_t ret = host_read(guest_to_host(addr), len);
    if (mem_trace.record_read && mem_trace.file)
    {
        fprintf(mem_trace.file, "r 0x%016lx %d ", addr, len);
        switch(len){
            case 1: fprintf(mem_trace.file, "%02x\n", (uint8_t)ret);
            break;
            case 2: fprintf(mem_trace.file, "%04x\n", (uint16_t)ret);
            break;
            case 4: fprintf(mem_trace.file, "%08x\n", (uint32_t)ret);
            break;
            case 8: fprintf(mem_trace.file, "%016lx\n", (uint64_t)ret);
            break;
            default: assert(0);
            break;
        }
    }
    return ret;
error:
    return 0;
}

void mem_write(uint64_t addr, int len, uint64_t data){
    check(addr - MEM_BASE < MEM_SIZE, "Write addr %016lx out of bound. PC %08lx", addr, cpu.pc);
    host_write(guest_to_host(addr), len, data);
    if (mem_trace.record_write && mem_trace.file)
    {
        fprintf(mem_trace.file, "w 0x%016lx %d ", addr, len);
        switch(len){
            case 1: fprintf(mem_trace.file, "%02x\n", (uint8_t)data);
            break;
            case 2: fprintf(mem_trace.file, "%04x\n", (uint16_t)data);
            break;
            case 4: fprintf(mem_trace.file, "%08x\n", (uint32_t)data);
            break;
            case 8: fprintf(mem_trace.file, "%016lx\n", (uint64_t)data);
            break;
            default: assert(0);
            break;
        }
    }
error:
    return;
}

void enable_mem_trace()
{
    mem_trace.file = fopen("memtrace.out", "w");
    assert(mem_trace.file);
    mem_trace.record_read = 1;
    mem_trace.record_write = 1;
}

void load_image(char *filepath){
    log_info("Physical Memory Range:[%016x, %016x].", MEM_BASE, MEM_BASE + MEM_SIZE - 1);
    check(filepath[0] != '\0', "IMAGE file path wrong.");
    FILE *fp = fopen(filepath, "rb");
    check(fp, "Failed to read %s.", filepath);
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    log_info("The image is %s, size = %ld.", filepath, size);
    fseek(fp, 0, SEEK_SET);
    int ret = fread(guest_to_host(MEM_BASE), size, 1, fp);
    check(ret == 1, "Load image failed.");
    fclose(fp);
    return;

error:
    if(fp) fclose(fp);
    return;
}
