#include <string>
#include <unordered_map>
#include <cstring>
#include <cassert>

#include "rv64/asm.h"
extern "C" {
    #include "arch_perf.h"
}

/**Static Data */
static std::unordered_map<RV64Ins, int> multicycle_map = {
    // RV64I Load Instructions (5 cycles: F + D + MemAdr + MemRead + WB)
    {LB, 5}, {LH, 5}, {LW, 5}, {LD, 5}, {LBU, 5}, {LHU, 5}, {LWU, 5},
    
    // RV64I Store Instructions (4 cycles: F + D + MemAdr + MemWrite)
    {SB, 4}, {SH, 4}, {SW, 4}, {SD, 4},
    
    // RV64I Immediate & Register-Register (4 cycles: F + D + Execute + WB)
    {ADDI, 4}, {SLTI, 4}, {SLTIU, 4}, {ANDI, 4}, {ORI, 4}, {XORI, 4},
    {ADDIW, 4}, {SLLI, 4}, {SRLI, 4}, {SRAI, 4}, {SLLIW, 4}, {SRLIW, 4}, {SRAIW, 4},
    {ADD, 4}, {SUB, 4}, {SLT, 4}, {SLTU, 4}, {AND, 4}, {OR, 4}, {XOR, 4},
    {SLL, 4}, {SRL, 4}, {SRA, 4}, {ADDW, 4}, {SUBW, 4}, {SLLW, 4}, {SRLW, 4}, {SRAW, 4},
    
    // RV64I Branch (3 cycles: F + D + Execute)
    {BEQ, 3}, {BNE, 3}, {BLT, 3}, {BGE, 3}, {BLTU, 3}, {BGEU, 3},
    
    // RV64I Jump (JAL=3 cycles no EX, JALR=4 cycles)
    {JAL, 3}, {JALR, 4},
    
    // RV64I Upper Immediate (4 cycles: F + D + ExecuteImm + WB)
    {LUI, 4}, {AUIPC, 4},
    
    // RV64M Multiplication (64-bit=5 cycles, 32-bit=4 cycles)
    {MUL, 5}, {MULH, 5}, {MULHSU, 5}, {MULHU, 5}, {MULW, 4},
    
    // RV64M Division/Remainder (43 cycles: F + D + 40 Execute + WB)
    {DIV, 43}, {DIVU, 43}, {REM, 43}, {REMU, 43},
    {DIVW, 43}, {DIVUW, 43}, {REMW, 43}, {REMUW, 43},
    
    // System Instructions (default to 4 cycles)
    {ECALL, 4}, {EBREAK, 4}, {UNK, 4}
};

/**Class Declaration */
enum class PerfProfilerType
{
    NONE,
    MULTICYCLE,
    PIPELINE,
};


class BasePerfProfiler
{
public:
    virtual size_t get_cycle_count() const;
    virtual size_t get_instruction_count() const;
    virtual void record_instruction(RV64DecodedIns ins) = 0;
    virtual ~BasePerfProfiler() = default;
    BasePerfProfiler(PerfProfilerType type) : type(type) {}
public:
    const PerfProfilerType type;
protected:
    size_t cycle_count = 0;
    size_t instruction_count = 0;
};

class MulticyclePerfProfiler : public BasePerfProfiler
{
public:
    MulticyclePerfProfiler();
    void record_instruction(RV64DecodedIns ins) override;
    
protected:
    RV64DecodedIns last_ins_decoded;
};

/**Class definition */
size_t BasePerfProfiler::get_cycle_count() const
{
    return cycle_count;
}

size_t BasePerfProfiler::get_instruction_count() const
{
    return instruction_count;
}

MulticyclePerfProfiler::MulticyclePerfProfiler() : BasePerfProfiler(PerfProfilerType::MULTICYCLE)
{
    cycle_count = 0;
    instruction_count = 0;
    last_ins_decoded = {UNK, TYPE_N, {0}};
}

void MulticyclePerfProfiler::record_instruction(RV64DecodedIns ins_decoded)
{
    auto it = multicycle_map.find(ins_decoded.ins);
    if (it != multicycle_map.end())
    {
        cycle_count += it->second;
        if (last_ins_decoded.ins == DIV && ins_decoded.ins == REM &&
            last_ins_decoded.R.rd != ins_decoded.R.rs1 &&
            last_ins_decoded.R.rs1 != ins_decoded.R.rd ||
            last_ins_decoded.ins == DIVU && ins_decoded.ins == REMU &&
            last_ins_decoded.R.rd != ins_decoded.R.rs1 &&
            last_ins_decoded.R.rs1 != ins_decoded.R.rd ||
            last_ins_decoded.ins == DIVW && ins_decoded.ins == REMW &&
            last_ins_decoded.R.rd != ins_decoded.R.rs1 &&
            last_ins_decoded.R.rs1 != ins_decoded.R.rd ||
            last_ins_decoded.ins == DIVUW && ins_decoded.ins == REMUW &&
            last_ins_decoded.R.rd != ins_decoded.R.rs1 &&
            last_ins_decoded.R.rs1 != ins_decoded.R.rd)
        {
            cycle_count -= 40; // Reduce the cycle count for the second instruction
        }
    }
    else
    {
        cycle_count += 1; // Default cycle count for unknown instructions
    }
    instruction_count++;
}


/**External API */
static BasePerfProfiler *perf_profiler = nullptr;

extern "C"
{
int set_perf_profiler(const char *arch)
{
    if (!arch)
    {
        if (perf_profiler)
        {
            delete perf_profiler;
            perf_profiler = nullptr;
        }
        return static_cast<int>(PerfProfilerType::NONE);
    }
    else if (!strcmp(arch, "multicycle"))
    {
        if (perf_profiler)
        {
            delete perf_profiler;
        }
        perf_profiler = new MulticyclePerfProfiler();
        return static_cast<int>(PerfProfilerType::MULTICYCLE);
    }
    else if (!strcmp(arch, "pipeline"))
    {
        if (perf_profiler)
        {
            delete perf_profiler;
        }
        assert(false && "Pipeline performance profiler is not implemented yet.");
    }
    else
    {
        assert(false && "Unknown performance profiler.");
    }
}

void perf_record_instruction(RV64DecodedIns ins_decoded)
{
    if (perf_profiler)
    {
        perf_profiler->record_instruction(ins_decoded);
    }
}

size_t perf_get_cycle_count()
{
    return perf_profiler ? perf_profiler->get_cycle_count() : -1;
}

size_t perf_get_instruction_count()
{
    return perf_profiler ? perf_profiler->get_instruction_count() : -1;
}

int get_perf_profiler_type()
{
    if (perf_profiler)
    {
        return static_cast<int>(perf_profiler->type);
    }
    else
    {
        return static_cast<int>(PerfProfilerType::NONE);
    }
}
}