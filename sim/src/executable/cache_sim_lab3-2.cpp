// This excutable is specifically for lab3-2

#include "cache.hpp"
#include <iostream>
#include <cstring>
#include <fstream>

static const char * USE_HELP =
"Usage: CacheSimulator"
" trace_path\n"
" --opt: Use an enhanced cache(with prefetch and other optimizations, details in report)\n";

int main(int argc, char *argv[])
{
    if (argc > 3)
    {
        std::cerr << USE_HELP << std::endl;
        return 1;
    }


    CacheConfig Dram_config{0, 0, 0, EvictPolicy::LRU, WritePolicy::WriteBack, WriteMissPolicy::WriteAllocate, 100, 20};
    CacheConfig L2_config{262144, 64, 8, EvictPolicy::LRU, WritePolicy::WriteBack, WriteMissPolicy::WriteAllocate, 3, 6};
    CacheConfig L1_config{32768, 64, 8, EvictPolicy::LRU, WritePolicy::WriteBack, WriteMissPolicy::WriteAllocate, 2, 0};
    std::unique_ptr<CacheInterface> dram = std::make_unique<Memory>(Dram_config);
    std::unique_ptr<CacheInterface> l1;

    if (argc == 3)
    {
        if (strcmp(argv[2], "--opt") == 0)
        {
            std::unique_ptr<CacheInterface> l2 = std::make_unique<CacheNextPretch>(L2_config, 4, std::move(dram));
            l1 = std::make_unique<CacheNextPretch>(L1_config, 4, std::move(l2));
        }
        else
        {
            std::cerr << "Error: Invalid option." << std::endl;
            return 1;
        }
    }
    else
    {
        std::unique_ptr<CacheInterface> l2 = std::make_unique<Cache>(L2_config, std::move(dram));
        l1 = std::make_unique<Cache>(L1_config, std::move(l2));
    }


    size_t total_latency = 0;
    size_t total_access = 0;
    auto ifs = std::ifstream(argv[1]);
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
        total_access++;
        char op;
        unsigned long long addr;
        std::sscanf(line.c_str(), "%c %llx", &op, &addr);
        if (op == 'r')
        {
            total_latency += l1->read(addr, 1);
        }
        else if (op == 'w')
        {
            total_latency += l1->write(addr, 1);
        }
        else
        {
            std::cerr << "Error: Invalid operation in trace file." << std::endl;
            return 1;
        }
    }
    std::printf("L1 Total Reads: %zu\n", l1->get_read_count());
    std::printf("L2 Total Read Misses: %zu\n", l1->get_read_miss_count());
    std::printf("L1Total Writes: %zu\n", l1->get_write_count());
    std::printf("L1 Total Write Misses: %zu\n", l1->get_write_miss_count());
    std::printf("L1 Miss Rate: %.2f%%\n", l1->get_miss_rate() * 100);
    std::printf("L1 Extra Info:\n");
    l1->print_extra_info();
    std::printf("L2 Total Reads: %zu\n", l1->get_next_cache()->get_read_count());
    std::printf("L2 Total Read Misses: %zu\n", l1->get_next_cache()->get_read_miss_count());
    std::printf("L2 Total Writes: %zu\n", l1->get_next_cache()->get_write_count());
    std::printf("L2 Total Write Misses: %zu\n", l1->get_next_cache()->get_write_miss_count());
    std::printf("L2 Miss Rate: %.2f%%\n", l1->get_next_cache()->get_miss_rate() * 100);
    std::printf("L2 Extra Info:\n");
    l1->get_next_cache()->print_extra_info();
    std::printf("Total Latency: %zu\n", total_latency);
    std::printf("Average Latency: %.2f\n", (double)total_latency / total_access);
}