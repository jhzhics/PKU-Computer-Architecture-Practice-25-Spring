#include "rv64/asm.h"


int try_get_imm(RV64DecodedIns const *ins_decoded, int64_t *imm)
{
    if (!ins_decoded)
    {
        return -1; // Invalid arguments
    }
    switch (ins_decoded->type)
    {
        case TYPE_I:
            if (imm)
            *imm = ins_decoded->I.imm;
            break;
        case TYPE_U:
            if (imm)
            *imm = ins_decoded->U.imm;
            break;
        case TYPE_S:
            if (imm)
            *imm = ins_decoded->S.imm;
            break;
        case TYPE_SB:
            if (imm)
            *imm = ins_decoded->SB.imm;
            break;
        case TYPE_J:
            if (imm)
            *imm = ins_decoded->J.imm;
            break;
        default:
            return -1; // No immediate value
    }
    return 0;
}

int try_get_rs1(RV64DecodedIns const *ins_decoded, int *rs1)
{
    if (!ins_decoded)
    {
        return -1; // Invalid arguments
    }
    switch (ins_decoded->type)
    {
        case TYPE_I:
            if (rs1)
            *rs1 = ins_decoded->I.rs1;
            break;
        case TYPE_S:
            if (rs1)
            *rs1 = ins_decoded->S.rs1;
            break;
        case TYPE_R:
            if (rs1)
            *rs1 = ins_decoded->R.rs1;
            break;
        case TYPE_SB:
            if (rs1)
            *rs1 = ins_decoded->SB.rs1_val;
            break;
        default:
            return -1; // No source register
    }
    return 0;
}

int try_get_rs2(RV64DecodedIns const *ins_decoded, int *rs2)
{
    if (!ins_decoded)
    {
        return -1; // Invalid arguments
    }
    switch (ins_decoded->type)
    {
        case TYPE_R:
            if(rs2)
            *rs2 = ins_decoded->R.rs2;
            break;
        case TYPE_S:
            if(rs2)
            *rs2 = ins_decoded->S.rs2;
            break;
        case TYPE_SB:
            if(rs2)
            *rs2 = ins_decoded->SB.rs2_val;
            break;
        default:
            return -1; // No second source register
    }
    return 0;
}

int try_get_rd(RV64DecodedIns const *ins_decoded, int *rd)
{
    if (!ins_decoded)
    {
        return -1; // Invalid arguments
    }
    switch (ins_decoded->type)
    {
        case TYPE_I:
            if(rd)
            *rd = ins_decoded->I.rd;
            break;
        case TYPE_U:
            if(rd)
            *rd = ins_decoded->U.rd;
            break;
        case TYPE_R:
            if(rd)
            *rd = ins_decoded->R.rd;
            break;
        case TYPE_J:
            if(rd)
            *rd = ins_decoded->J.rd;
            break;
        default:
            return -1; // No destination register
    }
    return 0;
}

int is_branch_taken(RV64DecodedIns const *ins_decoded, int *branch_taken)
{
    if (!ins_decoded || ins_decoded->type != TYPE_SB)
    {
        return -1;
    }
    if (branch_taken)
    {
        *branch_taken = 0;
        switch (ins_decoded->ins)
        {
            case BEQ:
                *branch_taken = (ins_decoded->SB.rs1_val == ins_decoded->SB.rs2_val);
                break;
            case BNE:
                *branch_taken = (ins_decoded->SB.rs1_val != ins_decoded->SB.rs2_val);
                break;
            case BLT:
                *branch_taken = ((int64_t)ins_decoded->SB.rs1_val < (int64_t)ins_decoded->SB.rs2_val);
                break;
            case BGE:
                *branch_taken = ((int64_t)ins_decoded->SB.rs1_val >= (int64_t)ins_decoded->SB.rs2_val);
                break;
            case BLTU:
                *branch_taken = (ins_decoded->SB.rs1_val < ins_decoded->SB.rs2_val);
                break;
            case BGEU:
                *branch_taken = (ins_decoded->SB.rs1_val >= ins_decoded->SB.rs2_val);
                break;
            default:
                return -1; // Not a branch instruction
        }
    }
    return 0;
}
