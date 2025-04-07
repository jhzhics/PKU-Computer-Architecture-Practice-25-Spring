#include <common.h>
#include <decode.h>
#include <mem.h>
#include <state.h>
#include <arch_perf.h>

void init_cpu(){
    cpu.pc = MEM_BASE;
    cpu.reg[0] = 0;
}


void exec_once(){
    Decode s;
    s.pc   = cpu.pc;
    s.inst = inst_fetch(s.pc);
    s.snpc = s.pc + 4;
    decode_exec(&s);
    cpu.pc = s.dnpc;
}

void cpu_exec(){
    while(running){
        exec_once();
    }
}

static void good_trap_exit()
{
    if (get_perf_profiler_type() != 0) {
        size_t dyn_insns = perf_get_instruction_count();
        size_t dyn_cycles = perf_get_cycle_count();
        printf(ANSI_FMT("HIT GOOD TRAP!\n", ANSI_FG_GREEN));
        printf("Performance Profiler: %s\n", get_perf_profiler_type() == 1 ? "Multicycle" : "Pipeline");
        printf("Dynamic instructions: %zu\n", dyn_insns);
        printf("Dynamic cycles: %zu\n", dyn_cycles);
        printf("CPI: %.2f\n", (float)dyn_cycles / dyn_insns);
        print_misc_perf_info();
    }
    else
    {
        printf(ANSI_FMT("HIT GOOD TRAP!\n", ANSI_FG_GREEN));
    }
}


void halt_trap(uint64_t pc, uint64_t code){
    if(code){
        printf(ANSI_FMT("HIT BAD TRAP!\n", ANSI_FG_RED));
    }else{
        good_trap_exit();
    }
    log_info("Program ended at pc %08lx, with exit code %ld.", pc, code);
    running = 0;
}
