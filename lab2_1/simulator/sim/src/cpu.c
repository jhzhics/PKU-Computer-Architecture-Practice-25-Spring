#include <common.h>
#include <decode.h>
#include <memory.h>
#include <state.h>

void init_cpu(){
    cpu.pc = MEM_BASE;
    cpu.reg[0] = 0;
}


static void exec_once(){
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

void halt_trap(uint64_t pc, uint64_t code){
    if(code){
        printf(ANSI_FMT("HIT BAD TRAP!\n", ANSI_FG_RED));
    }else{
        printf(ANSI_FMT("HIT GOOD TRAP!\n", ANSI_FG_GREEN));
    }
    log_info("Program ended at pc %08lx, with exit code %ld.", pc, code);
    running = 0;
}
