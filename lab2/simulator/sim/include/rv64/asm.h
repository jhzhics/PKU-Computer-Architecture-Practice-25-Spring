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

    // Special
    UNK
};

typedef struct {
  enum RV64Ins ins;
  enum RV64Type type;
  union {
    struct {
      uint64_t imm;
      uint64_t rs1;
      uint64_t rd;
    } I;
    struct {
      uint64_t imm;
      uint64_t rd;
    } U;
    struct {
      uint64_t imm;
      uint64_t rs1;
      uint64_t rs2;
    } S;
    struct {
      uint64_t rd;
      uint64_t rs1;
      uint64_t rs2;
    } R;
    struct {
      uint64_t imm;
      uint64_t rs1;
      uint64_t rs2;
    } SB;
    struct {
      uint64_t imm;
      uint64_t rd;
    } J;
    struct {
    } N;
  };
} RV64DecodedIns;

#endif