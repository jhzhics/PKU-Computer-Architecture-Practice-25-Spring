#ifndef __ARCH_PERF_H__
#define __ARCH_PERF_H__
#include "rv64/asm.h"
#include "stddef.h"

/**
 * @brief Activate the performance profiler.
 * @param arch The architecture type. If NULL, the profiler is disabled.
 *            If "multicycle", the multicycle profiler is activated.
 *           If "pipeline", the pipeline profiler is activated.
 * @returns The type of the performance profiler. 0 for none, 1 for multicyle, 2 for pipeline.
 */
int set_perf_profiler(const char *arch);
void perf_record_instruction(RV64DecodedIns ins_decoded);
size_t perf_get_cycle_count();
size_t perf_get_instruction_count();
int get_perf_profiler_type();
#endif