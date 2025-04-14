#ifndef __CACHE_H__
#define __CACHE_H__
#include "stddef.h"

enum EvictPolicy
{
    LRU
};

/**
 * @brief Cache write policy. Plus,we always use write allocate.
 */
enum WritePolicy
{
    WriteThrough,
    WriteBack
};

enum WriteMissPolicy
{
    WriteAllocate,
    NoWriteAllocate
};

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

#endif