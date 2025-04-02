#include <string>
#include <unordered_map>
#include <cstring>
#include <cassert>
#include <algorithm>
#include <cstdio>

extern "C" {
    #include "rv64/asm.h"
    #include "arch_perf.h"
}

#include "arch.hpp"

/**Static Data */

// The cycle count for each instruction in multicycle mode
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
    
    // System Instructions
    {ECALL, 4}, {EBREAK, 4}, 
    
    {NOP, 1},
    {UNK, INT64_MIN}
};


// The excution cycle count for each instruction in pipeline mode
static std::unordered_map<RV64Ins, int> pipeline_map = {
    // 64-bit multiplication needs 2 cycles
    {MUL, 2}, {MULH, 2}, {MULHSU, 2}, {MULHU, 2},
    
    // div/rem needs 40 cycles
    {DIV, 40}, {DIVU, 40}, {REM, 40}, {REMU, 40},
    {DIVW, 40}, {DIVUW, 40}, {REMW, 40}, {REMUW, 40},
    
    // other instructions need 1 cycles
    {LB, 1}, {LH, 1}, {LW, 1}, {LD, 1}, {LBU, 1}, {LHU, 1}, {LWU, 1},
    {SB, 1}, {SH, 1}, {SW, 1}, {SD, 1},
    {ADDI, 1}, {SLTI, 1}, {SLTIU, 1}, {ANDI, 1}, {ORI, 1}, {XORI, 1},
    {ADDIW, 1}, {SLLI, 1}, {SRLI, 1}, {SRAI, 1}, {SLLIW, 1}, {SRLIW, 1}, {SRAIW, 1},
    {ADD, 1}, {SUB, 1}, {SLT, 1}, {SLTU, 1}, {AND, 1}, {OR, 1}, {XOR, 1},
    {SLL, 1}, {SRL, 1}, {SRA, 1}, {ADDW, 1}, {SUBW, 1}, {SLLW, 1}, {SRLW, 1}, {SRAW, 1},
    {BEQ, 1}, {BNE, 1}, {BLT, 1}, {BGE, 1}, {BLTU, 1}, {BGEU, 1},
    {JAL, 1}, {JALR, 1},
    {LUI, 1}, {AUIPC, 1},
    
    {MULW, 1},
    
    {ECALL, 1}, {EBREAK, 1}, 
    
    {NOP, 1},
    // This instruction acts as 
    {UNK, 0}
};


/**Class definition */
size_t BasePerfProfiler::get_cycle_count()
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
        if ((last_ins_decoded.ins == DIV && ins_decoded.ins == REM ||
            last_ins_decoded.ins == DIVU && ins_decoded.ins == REMU ||
            last_ins_decoded.ins == DIVW && ins_decoded.ins == REMW ||
            last_ins_decoded.ins == DIVUW && ins_decoded.ins == REMUW) &&
            last_ins_decoded.R.rs1 == ins_decoded.R.rs1 &&
            last_ins_decoded.R.rs2 == ins_decoded.R.rs2 &&
            last_ins_decoded.R.rd != ins_decoded.R.rs1 &&
            last_ins_decoded.R.rd != ins_decoded.R.rs2)
        {
            cycle_count -= 40; // Reduce the cycle count for the second instruction
        }
    }
    else
    {
        cycle_count += 1; // Default cycle count for unknown instructions
    }
    instruction_count++;
    last_ins_decoded = ins_decoded;
}

PipelinePerfProfiler::PipelinePerfProfiler() : BasePerfProfiler(PerfProfilerType::PIPELINE){
    cycle_count = 0;
    instruction_count = 0;
    std::fill_n(hazards_cnt.begin(), HAZARD_TYPEN, 0);
    std::fill_n(pipeline_stages.begin(), BASE_PIPELINE_PHASE_N, RV64DecodedIns{UNK, TYPE_N, {0}});
}

void PipelinePerfProfiler::record_instruction(RV64DecodedIns ins_decoded)
{
    while (!pipeline_stages.if_can_issue())
    {
        cycle_count++;
        pipeline_stages.next_clock();
    }
    pipeline_stages.issue(ins_decoded);
    instruction_count++;
}

size_t PipelinePerfProfiler::get_cycle_count()
{
    cycle_count += pipeline_stages.flush();
    return cycle_count;
}

void PipelinePerfProfiler::print_misc_perf_info() const
{
    printf("Control Hazard Stall Cycles: %zu\n", pipeline_stages.get_control_hazard_stall());
    printf("Data Hazard Stall Cycles: %zu\n", pipeline_stages.get_data_hazard_stall());
}

RV64DecodedIns &__PipelineStages::IF()
{
    return std::array<RV64DecodedIns, PHASE_N>::operator[](begin_idx);
}

RV64DecodedIns &__PipelineStages::ID()
{
    return std::array<RV64DecodedIns, PHASE_N>::operator[](
        (begin_idx + 1) % PHASE_N);
}

RV64DecodedIns &__PipelineStages::EX()
{
    return std::array<RV64DecodedIns, PHASE_N>::operator[](
        (begin_idx + 2) % PHASE_N);
}

RV64DecodedIns &__PipelineStages::MEM()
{
    return std::array<RV64DecodedIns, PHASE_N>::operator[](
        (begin_idx + 3) % PHASE_N);
}

RV64DecodedIns &__PipelineStages::WB()
{
    return std::array<RV64DecodedIns, PHASE_N>::operator[](
        (begin_idx + 4) % PHASE_N);
}

bool __PipelineStages::is_hazard()
{
    assert(EX_left <= 0);
    /* Data Hazard */
    int rs1 = 0, rs2 = 0, rd = 0;
    try_get_rs1(&this->IF(), &rs1);
    try_get_rs2(&this->IF(), &rs2);
    // ID
    if (try_get_rd(&this->ID(), &rd) == 0 && rd)
    {
        if (rd == rs1 || rd == rs2)
        {
            data_hazard_stall++;
            return true;
        }
    }
    // EX
    rd = 0;
    if (try_get_rd(&this->EX(), &rd) == 0 && rd)
    {
        if (rd == rs1 || rd == rs2)
        {
            data_hazard_stall++;
            return true;
        }
    }
    // jalr hazard
    if (this->ID().ins == JALR || this->ID().ins == JAL)
    {
        data_hazard_stall++;
        return true;
    }

    /* Control Hazard */
    int takenQ = -1;
    // ID
    if (is_branch_taken(&this->ID(), &takenQ) == 0 && takenQ)
    {
        control_hazard_stall++;
        return true;
    }
    // EX
    if (is_branch_taken(&this->EX(), &takenQ) == 0 && takenQ)
    {
        control_hazard_stall++;
        return true;
    }

    return false;
}

RV64DecodedIns &__PipelineStages::operator[](size_t i)
{
    return std::array<RV64DecodedIns, PHASE_N>::operator[](
    (i + begin_idx) % PHASE_N);
}
const RV64DecodedIns & __PipelineStages::operator[](size_t i) const
{
    return std::array<RV64DecodedIns, PHASE_N>::operator[](
        (i + begin_idx) % PHASE_N);
}
RV64DecodedIns const &__PipelineStages::IF() const
{
    return std::array<RV64DecodedIns, PHASE_N>::operator[](begin_idx);
}
RV64DecodedIns const& __PipelineStages::ID() const
{
    return std::array<RV64DecodedIns, PHASE_N>::operator[](
        (begin_idx + 1) % PHASE_N);
}
RV64DecodedIns const& __PipelineStages::EX() const
{
    return std::array<RV64DecodedIns, PHASE_N>::operator[](
        (begin_idx + 2) % PHASE_N);
}
RV64DecodedIns const& __PipelineStages::MEM() const
{
    return std::array<RV64DecodedIns, PHASE_N>::operator[](
        (begin_idx + 3) % PHASE_N);
}
RV64DecodedIns const& __PipelineStages::WB() const
{
    return std::array<RV64DecodedIns, PHASE_N>::operator[](
        (begin_idx + 4) % PHASE_N);
}

/**
 * @brief Move the pipeline to the next clock cycle
 * @returns true if the pipeline has cleared the IF stage(i.e. can issue a new instruction)
 */
bool __PipelineStages::next_clock()
{
    if (--EX_left <= 0)
    {
        
        if (is_hazard())
        {
            begin_idx = (begin_idx + PHASE_N - 1) % PHASE_N;
            this->IF() = this->ID();
            this->ID() = {NOP, TYPE_N, {0}};
            
        }
        else
        {
            begin_idx = (begin_idx + PHASE_N - 1) % PHASE_N;
            this->IF() = {UNK, TYPE_N, {0}};
        }
        EX_left = pipeline_map[this->EX().ins];

        // Special case for div/rem pairs - if MEM is div and EX is rem, set EX_left to 1
        if ((this->MEM().ins == DIV && this->EX().ins == REM ||
            this->MEM().ins == DIVU && this->EX().ins == REMU ||
            this->MEM().ins == DIVW && this->EX().ins == REMW ||
            this->MEM().ins == DIVUW && this->EX().ins == REMUW) &&
            this->MEM().R.rs1 == this->EX().R.rs1 &&
            this->MEM().R.rs2 == this->EX().R.rs2 &&
            this->MEM().R.rd != this->EX().R.rs1 &&
            this->MEM().R.rd != this->EX().R.rs2) {
            
            // DIV and REM can share execution cycles, so REM only needs 1 more cycle
            EX_left = 1;
        }
        }
    return this->if_can_issue();
}

int __PipelineStages::flush()
{
    int cycles = 0;
    while(
        this->IF().ins != UNK || this->ID().ins != UNK ||
        this->EX().ins != UNK || this->MEM().ins != UNK ||
        this->WB().ins != UNK)
    {
        cycles++;
        this->next_clock();
    }
    return cycles;
}

void __PipelineStages::issue(const RV64DecodedIns &ins)
{
    assert(this->if_can_issue());
    this->IF() = ins;
}

bool __PipelineStages::if_can_issue() const
{
    return this->IF().ins == UNK;
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
        perf_profiler = new PipelinePerfProfiler();
        return static_cast<int>(PerfProfilerType::PIPELINE);
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
void print_misc_perf_info()
{
    if (perf_profiler)
    {
        perf_profiler->print_misc_perf_info();
    }
}
}
