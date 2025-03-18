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
  TYPE_N, TYPE_J, TYPE_SB,
  TYPE_R
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
#define immSB() do { *imm = SEXT(BITS(i, 31, 31) << 12 | \
                                 BITS(i,  7, 7)  << 11 | \
                                 BITS(i, 30, 25) << 5  | \
                                 BITS(i, 11, 8)  << 1  , 13);} while(0)

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
        case TYPE_SB: src1R(); src2R(); immSB(); break;
        case TYPE_R: src1R(); src2R(); break;
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

    // RV64I: 加载指令 (opcode: 0000011)
    INSTPAT("??????? ????? ????? 000 ????? 0000011", lb,   I, R(rd) = SEXT(Mr(src1 + imm, 1), 8));   // 加载 8 位有符号
    INSTPAT("??????? ????? ????? 001 ????? 0000011", lh,   I, R(rd) = SEXT(Mr(src1 + imm, 2), 16));  // 加载 16 位有符号
    INSTPAT("??????? ????? ????? 010 ????? 0000011", lw,   I, R(rd) = SEXT(Mr(src1 + imm, 4), 32));  // 加载 32 位有符号
    INSTPAT("??????? ????? ????? 011 ????? 0000011", ld,   I, R(rd) = Mr(src1 + imm, 8));            // 加载 64 位
    INSTPAT("??????? ????? ????? 100 ????? 0000011", lbu,  I, R(rd) = Mr(src1 + imm, 1));            // 加载 8 位无符号
    INSTPAT("??????? ????? ????? 101 ????? 0000011", lhu,  I, R(rd) = Mr(src1 + imm, 2));            // 加载 16 位无符号
    INSTPAT("??????? ????? ????? 110 ????? 0000011", lwu,  I, R(rd) = Mr(src1 + imm, 4));            // 加载 32 位无符号

    // RV64I: 存储指令 (opcode: 0100011)
    INSTPAT("??????? ????? ????? 000 ????? 0100011", sb,   S, Mw(src1 + imm, 1, (uint8_t)(src2 & 0xff)));
    INSTPAT("??????? ????? ????? 001 ????? 0100011", sh,   S, Mw(src1 + imm, 2, src2 & 0xffff));     // 存储 16 位
    INSTPAT("??????? ????? ????? 010 ????? 0100011", sw,   S, Mw(src1 + imm, 4, src2 & 0xffffffff)); // 存储 32 位
    INSTPAT("??????? ????? ????? 011 ????? 0100011", sd,   S, Mw(src1 + imm, 8, src2));              // 存储 64 位

    // RV64I: 立即数运算 (opcode: 0010011)
    INSTPAT("??????? ????? ????? 000 ????? 0010011", addi, I, R(rd) = src1 + imm);                   // 加法
    INSTPAT("??????? ????? ????? 010 ????? 0010011", slti, I, {
      int64_t imm_sext = (int64_t)SEXT(BITS(s->inst, 31, 20), 12);
      R(rd) = (int64_t)src1 < imm_sext ? 1 : 0;
    });
    INSTPAT("??????? ????? ????? 011 ????? 0010011", sltiu,I, R(rd) = src1 < imm ? 1 : 0);           // 无符号小于置 1
    INSTPAT("??????? ????? ????? 111 ????? 0010011", andi, I, R(rd) = src1 & imm);                   // 与
    INSTPAT("??????? ????? ????? 110 ????? 0010011", ori,  I, R(rd) = src1 | imm);                   // 或
    INSTPAT("??????? ????? ????? 100 ????? 0010011", xori, I, R(rd) = src1 ^ imm);                   // 异或

    // RV64I: 32 位立即数运算 (opcode: 0011011)
    INSTPAT("??????? ????? ????? 000 ????? 0011011", addiw,I, R(rd) = (int64_t)((int32_t)(src1 & 0xffffffff) + (int32_t)imm)); // 32 位加法，结果符号扩展

    // RV64I: 立即数移位 (opcode: 0010011)
    INSTPAT("000000 ?????? ????? 001 ????? 0010011", slli, I, R(rd) = src1 << (imm & 0x3f));         // 左移（64 位，6 位移位量）
    INSTPAT("000000 ?????? ????? 101 ????? 0010011", srli, I, R(rd) = src1 >> (imm & 0x3f));         // 无符号右移
    INSTPAT("010000 ?????? ????? 101 ????? 0010011", srai, I, R(rd) = (uint64_t)((int64_t)src1 >> (imm & 0x3f))); // 有符号右移
   
    // RV64I: 32 位立即数移位 (opcode: 0011011)
    INSTPAT("0000000 ????? ????? 001 ????? 0011011", slliw,I, R(rd) = (int64_t)((int32_t)((uint32_t)(src1 & 0xffffffff) << (imm & 0x1f)))); // 32 位左移
    INSTPAT("0000000 ????? ????? 101 ????? 0011011", srliw,I, R(rd) = (int64_t)((int32_t)((uint32_t)(src1 & 0xffffffff) >> (imm & 0x1f)))); // 32 位无符号右移
    INSTPAT("0100000 ????? ????? 101 ????? 0011011", sraiw,I, R(rd) = (int64_t)((int32_t)(src1 & 0xffffffff) >> (imm & 0x1f))); // 32 位有符号右移

    // RV64I: 寄存器运算 (opcode: 0110011)
    INSTPAT("0000000 ????? ????? 000 ????? 0110011", add,  R, R(rd) = src1 + src2);                  // 加法
    INSTPAT("0100000 ????? ????? 000 ????? 0110011", sub,  R, R(rd) = src1 - src2);                  // 减法
    INSTPAT("0000000 ????? ????? 010 ????? 0110011", slt,  R, R(rd) = (int64_t)src1 < (int64_t)src2 ? 1 : 0); // 有符号小于置 1
    INSTPAT("0000000 ????? ????? 011 ????? 0110011", sltu, R, R(rd) = src1 < src2 ? 1 : 0);          // 无符号小于置 1
    INSTPAT("0000000 ????? ????? 111 ????? 0110011", and,  R, R(rd) = src1 & src2);                  // 与
    INSTPAT("0000000 ????? ????? 110 ????? 0110011", or,   R, R(rd) = src1 | src2);                  // 或
    INSTPAT("0000000 ????? ????? 100 ????? 0110011", xor,  R, R(rd) = src1 ^ src2);                  // 异或
    INSTPAT("0000000 ????? ????? 001 ????? 0110011", sll,  R, R(rd) = src1 << (src2 & 0x3f));        // 左移
    INSTPAT("0000000 ????? ????? 101 ????? 0110011", srl,  R, R(rd) = src1 >> (src2 & 0x3f));        // 无符号右移
    INSTPAT("0100000 ????? ????? 101 ????? 0110011", sra,  R, R(rd) = (uint64_t)((int64_t)src1 >> (src2 & 0x3f))); // 有符号右移

    // RV64I: 32 位寄存器运算 (opcode: 0111011)
    INSTPAT("0000000 ????? ????? 000 ????? 0111011", addw, R, R(rd) = (int64_t)((int32_t)(src1 & 0xffffffff) + (int32_t)(src2 & 0xffffffff))); // 32 位加法
    INSTPAT("0100000 ????? ????? 000 ????? 0111011", subw, R, R(rd) = (int64_t)((int32_t)(src1 & 0xffffffff) - (int32_t)(src2 & 0xffffffff))); // 32 位减法
    INSTPAT("0000000 ????? ????? 001 ????? 0111011", sllw, R, R(rd) = (int64_t)((int32_t)((uint32_t)(src1 & 0xffffffff) << (src2 & 0x1f)))); // 32 位左移
    INSTPAT("0000000 ????? ????? 101 ????? 0111011", srlw, R, R(rd) = (int64_t)((int32_t)((uint32_t)(src1 & 0xffffffff) >> (src2 & 0x1f)))); // 32 位无符号右移
    INSTPAT("0100000 ????? ????? 101 ????? 0111011", sraw, R, R(rd) = (int64_t)((int32_t)(src1 & 0xffffffff) >> (src2 & 0x1f))); // 32 位有符号右移

    // RV64I: 分支指令 (opcode: 1100011)
    INSTPAT("??????? ????? ????? 000 ????? 1100011", beq,  SB, {
      uint64_t target = s->pc + imm;
      if (src1 == src2) s->dnpc = target;
      else s->dnpc = s->snpc;
    });
    INSTPAT("??????? ????? ????? 001 ????? 1100011", bne,  SB, if (src1 != src2) s->dnpc = s->pc + imm);       // 不等跳转
    INSTPAT("??????? ????? ????? 100 ????? 1100011", blt,  SB, if ((int64_t)src1 < (int64_t)src2) s->dnpc = s->pc + imm); // 有符号小于跳转
    INSTPAT("??????? ????? ????? 101 ????? 1100011", bge,  SB, if ((int64_t)src1 >= (int64_t)src2) s->dnpc = s->pc + imm); // 有符号大于等于跳转
    INSTPAT("??????? ????? ????? 110 ????? 1100011", bltu, SB, if (src1 < src2) s->dnpc = s->pc + imm);        // 无符号小于跳转
    INSTPAT("??????? ????? ????? 111 ????? 1100011", bgeu, SB, if (src1 >= src2) s->dnpc = s->pc + imm);       // 无符号大于等于跳转

    // RV64I: 跳转指令 (opcode: 1101111, 1100111)
    INSTPAT("??????? ????? ????? ??? ????? 1101111", jal,  J, R(rd) = s->pc + 4; s->dnpc = s->pc + imm);     // 跳转并链接
    INSTPAT("??????? ????? ????? 000 ????? 1100111", jalr, I, R(rd) = s->pc + 4; s->dnpc = (src1 + imm) & ~1); // 跳转并链接（寄存器）

    // RV64I: 加载立即数 (opcode: 0110111, 0010111)
    INSTPAT("??????? ????? ????? ??? ????? 0110111", lui,  U, R(rd) = (int64_t)(int32_t)(imm & 0xfffff000));                     // 加载高位立即数
    INSTPAT("??????? ????? ????? ??? ????? 0010111", auipc,U, R(rd) = s->pc + imm);                          // PC 相对立即数加法

    // RV64I: 系统指令 (opcode: 1110011)
    INSTPAT("0000000 00000 00000 000 00000 1110011", ecall, N, if (!upcall()) halt_trap(s->pc ,1));                             // 系统调用（暂作为暂停）
    INSTPAT("0000000 00001 00000 000 00000 1110011", ebreak,N, HALT(s->pc, R(10)));                         // 断点

    // RV64M: 乘法指令 (opcode: 0110011, funct7: 0000001)
    INSTPAT("0000001 ????? ????? 000 ????? 0110011", mul,   R, R(rd) = src1 * src2);                         // 乘法（低 64 位）
    INSTPAT("0000001 ????? ????? 001 ????? 0110011", mulh,  R, R(rd) = (uint64_t)(((__int128)(int64_t)src1 * (int64_t)src2) >> 64)); // 有符号乘法（高 64 位）
    INSTPAT("0000001 ????? ????? 010 ????? 0110011", mulhsu,R, R(rd) = (uint64_t)(((__int128)(int64_t)src1 * (__uint128_t)src2) >> 64)); // 有符号×无符号（高 64 位）
    INSTPAT("0000001 ????? ????? 011 ????? 0110011", mulhu, R, R(rd) = (uint64_t)(((__uint128_t)src1 * (__uint128_t)src2) >> 64)); // 无符号乘法（高 64 位）

    
    // RV64M: 除法和余数 (opcode: 0110011, funct7: 0000001)
    INSTPAT("0000001 ????? ????? 100 ????? 0110011", div,   R, {
      int64_t a = (int64_t)src1;
      int64_t b = (int64_t)src2;
      if (b == 0) {
          R(rd) = -1;
      } else if (a == INT64_MIN && b == -1) {
          R(rd) = INT64_MIN;
      } else {
          R(rd) = a / b;
      }
    });
    INSTPAT("0000001 ????? ????? 101 ????? 0110011", divu,  R, R(rd) = (src2 == 0) ? UINT64_MAX : src1 / src2); // 无符号除法
    INSTPAT("0000001 ????? ????? 110 ????? 0110011", rem,   R, R(rd) = (src2 == 0) ? src1 : ((src1 == INT64_MIN && src2 == -1) ? 0 : (int64_t)src1 % (int64_t)src2)); // 有符号余数
    INSTPAT("0000001 ????? ????? 111 ????? 0110011", remu,  R, R(rd) = (src2 == 0) ? src1 : src1 % src2);     // 无符号余数

    // RV64M: 32 位乘除法 (opcode: 0111011, funct7: 0000001)
    INSTPAT("0000001 ????? ????? 000 ????? 0111011", mulw,  R, {
      int32_t s1 = (int32_t)(src1 & 0xffffffff);
      int32_t s2 = (int32_t)(src2 & 0xffffffff);
      R(rd) = (int64_t)(s1 * s2); // 自动符号扩展
    });
    INSTPAT("0000001 ????? ????? 100 ????? 0111011", divw,  R, { int32_t s1 = (int32_t)(src1 & 0xffffffff); int32_t s2 = (int32_t)(src2 & 0xffffffff); \
                                                               R(rd) = (s2 == 0) ? -1 : ((s1 == INT32_MIN && s2 == -1) ? (int64_t)INT32_MIN : (int64_t)(s1 / s2)); }); // 32 位有符号除法
    INSTPAT("0000001 ????? ????? 101 ????? 0111011", divuw, R, { uint32_t s1 = (uint32_t)(src1 & 0xffffffff); uint32_t s2 = (uint32_t)(src2 & 0xffffffff); \
                                                               R(rd) = (s2 == 0) ? UINT64_MAX : (int64_t)(s1 / s2); }); // 32 位无符号除法
    INSTPAT("0000001 ????? ????? 110 ????? 0111011", remw,  R, { int32_t s1 = (int32_t)(src1 & 0xffffffff); int32_t s2 = (int32_t)(src2 & 0xffffffff); \
                                                               R(rd) = (s2 == 0) ? s1 : ((s1 == INT32_MIN && s2 == -1) ? 0 : (int64_t)(s1 % s2)); }); // 32 位有符号余数
    INSTPAT("0000001 ????? ????? 111 ????? 0111011", remuw, R, { uint32_t s1 = (uint32_t)(src1 & 0xffffffff); uint32_t s2 = (uint32_t)(src2 & 0xffffffff); \
                                                               R(rd) = (s2 == 0) ? s1 : (int64_t)(s1 % s2); }); // 32 位无符号余数

    // Invalid Inst
    INSTPAT("??????? ????? ????? ??? ????? ????? ??", unk,   N, printf(ANSI_FMT("Unknown Inst! %08x\n", ANSI_FG_RED), s->inst); HALT(s->pc, -1));

    INSTPAT_END();

    R(0) = 0;

    return;
}