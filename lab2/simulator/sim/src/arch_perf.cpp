#include <cassert>
#include <cstring>
extern "C" {
    #include "arch_perf.h"
}

#include "arch.hpp"

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
        perf_profiler = new PipelinePerfProfiler(false);
        return static_cast<int>(PerfProfilerType::PIPELINE);
    }
    else if (!strcmp(arch, "pipeline_pro"))
    {
        if (perf_profiler)
        {
            delete perf_profiler;
        }
        perf_profiler = new PipelinePerfProfiler(true);
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
