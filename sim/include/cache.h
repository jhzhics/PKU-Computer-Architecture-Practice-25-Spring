#ifndef __CACHE_H__
#define __CACHE_H__
#include "stddef.h"

typedef enum
{
    LRU
} EvictPolicy;

/**
 * @brief Cache write policy. Plus,we always use write allocate.
 */
typedef enum
{
    WriteThrough,
    WriteBack
} WritePolicy;

typedef enum
{
    WriteAllocate,
    NoWriteAllocate
} WriteMissPolicy;

typedef struct
{
    size_t cache_size;
    size_t block_size;
    size_t associativity;
    EvictPolicy evict_policy;
    WritePolicy write_policy;
    WriteMissPolicy write_miss_policy;
    size_t hit_latency;  // in cycles
    size_t bus_latency; // in cycles
} CacheConfig;

void init_cache(int cache_sim);
size_t write_cache(size_t addr, size_t len);
size_t read_cache(size_t addr, size_t len);
int is_cache_enabled();
double get_L1_miss_rate();
double get_L2_miss_rate();
double get_LLC_miss_rate();
#endif