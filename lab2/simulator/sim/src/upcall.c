#include "upcall.h"
#include "state.h"
#include "mem.h"
#include "dbg.h"

#include <syscall.h>
#include <unistd.h>
int upcall()
{
    uint64_t a0 = cpu.reg[10]; // arg1, retval
    uint64_t a1 = cpu.reg[11]; // arg2
    uint64_t a2 = cpu.reg[12]; // arg3
    uint64_t a3 = cpu.reg[13]; // arg4
    uint64_t a4 = cpu.reg[14]; // arg5
    uint64_t a5 = cpu.reg[15]; // arg6
    uint64_t a7 = cpu.reg[17]; // arg7

    switch (a7)
    {
    case SYS_RV64_fstat:
        cpu.reg[10] = fstat(a0, guest_to_host(a1));
    break;
    case SYS_RV64_brk:
        if (a0 == 0)
        {
            cpu.reg[10] = prog_brk;
        }
        else if (a0 - MEM_BASE <= MEM_SIZE)
        {
            prog_brk = a0;
            cpu.reg[10] = prog_brk; 
        }
        else
        {
            cpu.reg[10] = prog_brk;
        }
    break;
    case SYS_RV64_write:
    {
        int fd = (int)a0;
        void *buf = guest_to_host(a1);
        size_t count = (size_t)a2;
        ssize_t ret = write(fd, buf, count);
        cpu.reg[10] = (uint64_t)ret; 
        break;
    }
    default:
        log_info("Unrecognized/Unimplemented ecall no %lu", a7);
        return 0;
    }
    return 1;
}