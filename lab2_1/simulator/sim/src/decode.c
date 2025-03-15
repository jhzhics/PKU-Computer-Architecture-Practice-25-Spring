#include <common.h>
#include <macro.h>
#include <pattern.h>
#include <cpu.h>
#include <memory.h>

extern CPU_state cpu;

#define R(i) (cpu.reg[i])
#define Mr mem_read
#define Mw mem_write
#define HALT(thispc, code) halt_trap(thispc, code)

enum {
  TYPE_I, TYPE_U, TYPE_S,
  TYPE_N, TYPE_J
};

#define src1R() do { *src1 = R(rs1); } while (0)
#define src2R() do { *src2 = R(rs2); } while (0)
#define immI() do { *imm = SEXT(BITS(i, 31, 20), 12); } while(0)
#define immU() do { *imm = SEXT(BITS(i, 31, 12), 20) << 12; } while(0)
#define immJ() do { *imm = SEXT(BITS(i, 31, 31) << 20 | \
                                BITS(i, 19, 12) << 12 | \
                                BITS(i, 20, 20) << 11 | \
                                BITS(i, 30, 21) << 1, 21);} while(0)
#define immS() do { *imm = (SEXT(BITS(i, 31, 25), 7) << 5) | BITS(i, 11, 7); } while(0)

static void decode_operand(Decode *s, int *rd, uint64_t *src1, uint64_t *src2, uint64_t *imm, int type){
    uint32_t i = s->inst;
    int rs1 = BITS(i, 19, 15);
    int rs2 = BITS(i, 24, 20);
    *rd     = BITS(i, 11,  7);
    switch(type) {
        case TYPE_I: src1R();          immI(); break;
        case TYPE_U:                   immU(); break;
        case TYPE_J:                   immJ(); break;
        case TYPE_S: src1R(); src2R(); immS(); break;
    }
}
void decode_exec(Decode *s){
    int rd = 0;
    uint64_t src1 = 0, src2 = 0, imm = 0;
    s->dnpc = s->snpc;

#define INSTPAT_INST(s) ((s)->inst)
#define INSTPAT_MATCH(s, name, type, ... /* execute body */ ) { \
  decode_operand(s, &rd, &src1, &src2, &imm, concat(TYPE_, type)); \
  __VA_ARGS__ ; \
}
    INSTPAT_START();
    //RV64IM
    INSTPAT("??????? ????? ????? ??? ????? 00101 11", auipc  , U, R(rd) = s->pc + imm);
    INSTPAT("??????? ????? ????? ??? ????? 11011 11", jal    , J, R(rd) = s->pc + 4, s->dnpc = s->pc + imm);
    INSTPAT("??????? ????? ????? 000 ????? 11001 11", jalr   , I, R(rd) = s->pc + 4, s->dnpc = (src1 + imm) & ~1);
    INSTPAT("??????? ????? ????? 011 ????? 01000 11", sd     , S, Mw(src1 + imm, 8, src2));
    INSTPAT("??????? ????? ????? 000 ????? 00100 11", addi   , I, R(rd) = src1 + imm);
    INSTPAT("0000000 00001 00000 000 00000 11100 11", ebreak , N, HALT(s->pc, R(10))); // R(10) is $a0

    //Invalid Opcode
    INSTPAT("??????? ????? ????? ??? ????? ????? ??", unk    , N, printf(ANSI_FMT("Unknown Inst!\n", ANSI_FG_RED)), HALT(s->pc, -1));
    INSTPAT_END();

    R(0) = 0;

    return;
}
