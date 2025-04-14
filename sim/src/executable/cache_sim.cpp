#include "cache.hpp"
#include <iostream>
#include <cstring>
#include <fstream>

static const char * USE_HELP =
"Usage: CacheSimulator"
" --[WriteBack|WriteThrough]"
" --[WriteAllocate|NoWriteAllocate]"
" --[LRU]"
" <cache_size>"
" <block_size>"
" <associativity>"
" trace_path\n";

static const CacheConfig MaimMemoryConfig = {
    0,
    0,
    0,
    LRU,
    WriteBack,
    WriteAllocate,
    200,
    20,
};

static CacheConfig parse_config(int argc, char *argv[])
{
    if (argc != 8)
    {
        std::cerr << USE_HELP << std::endl;
        exit(1);
    }
    CacheConfig config;
    if (!strcmp(argv[1], "--WriteBack"))
    {
        config.write_policy = WriteBack;
    }
    else if (!strcmp(argv[1], "--WriteThrough"))
    {
        config.write_policy = WriteThrough;
    }
    else
    {
        std::cerr << USE_HELP << std::endl;
        exit(1);
    }
    if (!strcmp(argv[2], "--WriteAllocate"))
    {
        config.write_miss_policy = WriteAllocate;
    }
    else if (!strcmp(argv[2], "--NoWriteAllocate"))
    {
        config.write_miss_policy = NoWriteAllocate;
    }
    else
    {
        std::cerr << USE_HELP << std::endl;
        exit(1);
    }
    if (!strcmp(argv[3], "--LRU"))
    {
        config.evict_policy = LRU;
    }
    else
    {
        std::cerr << USE_HELP << std::endl;
        exit(1);
    }
    config.cache_size = atoi(argv[4]);
    config.block_size = atoi(argv[5]);
    config.associativity = atoi(argv[6]);
    config.hit_latency = 3;
    config.bus_latency = 0;
    return config;
}

int main(int argc, char *argv[])
{
    auto config = parse_config(argc, argv);
    Cache cache(config, std::make_unique<Memory>(MaimMemoryConfig));
    size_t total_latency = 0;
    auto ifs = std::ifstream(argv[7]);
    if (!ifs.is_open())
    {
        std::cerr << "Error: Unable to open trace file." << std::endl;
        return 1;
    }
    std::string line;
    while (std::getline(ifs, line))
    {
        if (line.empty())
            continue;
        char op;
        unsigned long long addr;
        std::sscanf(line.c_str(), "%c %llx", &op, &addr);
        if (op == 'r')
        {
            total_latency += cache.read(addr, 1);
        }
        else if (op == 'w')
        {
            total_latency += cache.write(addr, 1);
        }
        else
        {
            std::cerr << "Error: Invalid operation in trace file." << std::endl;
            return 1;
        }
    }
    std::printf("Total Reads: %zu\n", cache.get_read_count());
    std::printf("Total Read Misses: %zu\n", cache.get_read_miss_count());
    std::printf("Total Writes: %zu\n", cache.get_write_count());
    std::printf("Total Write Misses: %zu\n", cache.get_write_miss_count());
    std::printf("Total Latency: %zu\n", total_latency);
}