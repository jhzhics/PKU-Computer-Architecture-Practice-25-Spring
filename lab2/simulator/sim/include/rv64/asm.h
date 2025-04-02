#ifndef __RV64_ASM_H__
#define __RV64_ASM_H__
#include <stdint.h>

enum RV64Type{
    TYPE_I, TYPE_U, TYPE_S,
    TYPE_N, TYPE_J, TYPE_SB,
    TYPE_R
  };
  
enum RV64Ins {
    // RV64I Load Instructions
    LB, LH, LW, LD, LBU, LHU, LWU,

    // RV64I Store Instructions
    SB, SH, SW, SD,

    // RV64I Immediate Instructions
    ADDI, SLTI, SLTIU, ANDI, ORI, XORI,
    ADDIW,
    SLLI, SRLI, SRAI, SLLIW, SRLIW, SRAIW,

    // RV64I Register-Register Instructions
    ADD, SUB, SLT, SLTU, AND, OR, XOR,
    SLL, SRL, SRA,

    // RV64I 32-bit Register Instructions
    ADDW, SUBW, SLLW, SRLW, SRAW,

    // RV64I Branch Instructions
    BEQ, BNE, BLT, BGE, BLTU, BGEU,

    // RV64I Jump Instructions
    JAL, JALR,

    // RV64I Upper Immediate Instructions
    LUI, AUIPC,

    // RV64I System Instructions
    ECALL, EBREAK,

    // RV64M Multiplication Instructions
    MUL, MULH, MULHSU, MULHU,

    // RV64M Division/Remainder Instructions
    DIV, DIVU, REM, REMU,

    // RV64M 32-bit Multiply/Divide Instructions
    MULW, DIVW, DIVUW, REMW, REMUW,

    // For Pipeline bubbles
    NOP,
    // Special
    UNK
};

typedef struct {
  enum RV64Ins ins;
  enum RV64Type type;
  union {
    struct {
      int64_t imm;
      int rs1;
      uint64_t rs1_val;
      int rd;
    } I;
    struct {
      int64_t imm;
      int rd;
    } U;
    struct {
      int64_t imm;
      int rs1;
      int rs2;
      uint64_t rs1_val;
      uint64_t rs2_val;
    } S;
    struct {
      int rd;
      int rs1;
      int rs2;
      uint64_t rs1_val;
      uint64_t rs2_val;
    } R;
    struct {
      int64_t imm;
      int rs1;
      int rs2;
      uint64_t rs1_val;
      uint64_t rs2_val;
    } SB;
    struct {
      int64_t imm;
      int rd;
    } J;
    struct {
    } N;
  };
} RV64DecodedIns;

/**
 * @returns 0 if the instruction has an immediate value
 */
int try_get_imm(RV64DecodedIns const *ins_decoded, int64_t *imm);

/**
 * @returns 0 if the instruction has a second source register
 */
int try_get_rs1(RV64DecodedIns const *ins_decoded, int *rs1);

/**
 * @returns 0 if the instruction has a destination register
 */
int try_get_rs2(RV64DecodedIns const *ins_decoded, int *rs2);

/**
 * @returns 0 if the instruction has a destination register
 */
int try_get_rd(RV64DecodedIns const *ins_decoded, int *rd);

/**
 * @param branch_taken set to 1 if the instruction if the branch is taken 0 if not
 * @returns 0 if the instruction is a branch instruction
 */
int is_branch_taken(RV64DecodedIns const *ins_decoded, int *branch_taken);

#endif