#ifndef __arch_hpp__
#define __arch_hpp__

#include <array>
#include "rv64/asm.h"

enum class PerfProfilerType
{
    NONE,
    MULTICYCLE,
    PIPELINE,
};


class BasePerfProfiler
{
public:
    virtual size_t get_cycle_count();
    virtual size_t get_instruction_count() const;
    virtual void record_instruction(RV64DecodedIns ins) = 0;
    virtual ~BasePerfProfiler() = default;
    inline BasePerfProfiler(PerfProfilerType type) : type(type) {}
    inline virtual void print_misc_perf_info() const {};
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


static constexpr size_t BASE_PIPELINE_PHASE_N = 5;

class __PipelineStages : public std::array<RV64DecodedIns, BASE_PIPELINE_PHASE_N>
{
protected:
    int begin_idx = 0;
    int EX_left = 1;
    int MEM_left = 1;
    static constexpr int PHASE_N = BASE_PIPELINE_PHASE_N;
    int control_hazard_stall = 0;
    int data_hazard_stall = 0;

    RV64DecodedIns & IF();
    RV64DecodedIns & ID();
    RV64DecodedIns & EX();
    RV64DecodedIns & MEM();
    RV64DecodedIns & WB();

    /**
     * @brief Check if the pipeline is hazard
     * @warning Only should be called when the pipeline can clear IF.
     */
    virtual bool is_hazard();

public:

    RV64DecodedIns &operator[](size_t i);
    const RV64DecodedIns &operator[](size_t i) const;
    RV64DecodedIns const& IF() const;
    RV64DecodedIns const& ID() const;
    RV64DecodedIns const& EX() const;
    RV64DecodedIns const& MEM() const;
    RV64DecodedIns const& WB() const;
    inline size_t get_control_hazard_stall() const { return control_hazard_stall; }
    inline size_t get_data_hazard_stall() const { return data_hazard_stall; }
    
    /**
     * @brief Move the pipeline to the next clock cycle
     * @returns true if the pipeline has cleared the IF stage(i.e. can issue a new instruction)
     */
    bool next_clock();
    
    /**
     * @returns The number of cycles
     */
    int flush();

    void issue(const RV64DecodedIns &ins);

    bool if_can_issue() const;
};

class PipelinePerfProfiler : public BasePerfProfiler
{
public:
    PipelinePerfProfiler(bool pro);
    void record_instruction(RV64DecodedIns ins) override;
    size_t get_cycle_count() override;
    void print_misc_perf_info() const override;
    virtual ~PipelinePerfProfiler() override;
protected:
    enum HazardType
    {
        RAW,
        CONTROL,
        DIV_STALL,
        MUL64_STALL,
        HAZARD_TYPEN
    };
    std::array<size_t, HAZARD_TYPEN> hazards_cnt;
    __PipelineStages * pipeline_stages;
    
};

class __PipelineStagesPro : public __PipelineStages
{
protected:
    static constexpr size_t PREDICTOR_TABLE_SIZE = 4096;
    static int get_pc_predictor_idx(uint64_t pc);
    enum class PredictorState
    {
        NONE,
        STRONG_TAKEN,
        WEAK_TAKEN,
        WEAK_NOT_TAKEN,
        STRONG_NOT_TAKEN,
    };
    static int to_taken(PredictorState state);
    static PredictorState state_transition(PredictorState state, bool taken);
    std::array<PredictorState, PREDICTOR_TABLE_SIZE> predictor_table{};
    bool is_hazard() override;

};


#endif