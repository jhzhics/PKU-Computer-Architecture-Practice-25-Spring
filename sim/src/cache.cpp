#include "cache.hpp"
#include <bitset>
#include <cassert>
#include <algorithm>
#include <iostream>
#include "cache.h"

static CacheInterface *backend_cache = nullptr;

static bool is_valid_cache_config(const CacheConfig &config)
{
    bool valid = true;
    std::bitset<sizeof(size_t) * 8> cache_size(config.cache_size);
    std::bitset<sizeof(size_t) * 8> block_size(config.block_size);
    std::bitset<sizeof(size_t) * 8> associativity(config.associativity);
    valid = cache_size.count() == 1 &&
            block_size.count() == 1 &&
            associativity.count() == 1 &&
            config.cache_size >= config.block_size &&
            config.associativity <= config.cache_size / config.block_size;
    return valid;
}

static size_t get_first_bit_pos(size_t x)
{
    size_t pos = 0;
    while (x > 0)
    {
        if (x & 1)
            return pos;
        x >>= 1;
        pos++;
    }
    return -1;
}

Cache::Cache(const CacheConfig &config, std::unique_ptr<CacheInterface> next_cache) : CacheInterface(config)
{
    assert(is_valid_cache_config(config) && "Invalid cache configuration");
    m_set_n = config.cache_size / config.block_size / config.associativity;
    m_tag_shift = get_first_bit_pos(config.block_size) + get_first_bit_pos(m_set_n);
    m_block_shift = get_first_bit_pos(config.block_size);
    m_cache_table.resize(m_set_n,
    std::list<CacheEntry>(config.associativity));
    m_next_cache = std::move(next_cache);
}

size_t Cache::write(size_t addr, size_t len)
{
    size_t latency = 0;
    len += addr % config.block_size;
    addr = addr - addr % config.block_size;
    while (len > 0)
    {
        size_t set_index = get_set_index(addr);
        size_t tag = get_tag(addr);
        size_t single_len = std::min(len, config.block_size);
        latency += single_write(set_index, tag, single_len);
        len -= single_len;
        addr += single_len;
    }
    assert(get_write_count() >= get_write_miss_count());
    return latency;
}

size_t Cache::read(size_t addr, size_t len)
{
    size_t latency = 0;
    len += addr % config.block_size;
    addr = addr - addr % config.block_size;
    while (len > 0)
    {
        size_t set_index = get_set_index(addr);
        size_t tag = get_tag(addr);
        size_t single_len = std::min(len, config.block_size);
        latency += single_read(set_index, tag, single_len);
        len -= single_len;
        addr += single_len;
    }
    assert(get_read_count() >= get_read_miss_count());
    return latency;
}

size_t Cache::get_set_index(size_t addr) const
{
    return (addr >> m_block_shift) & (m_set_n - 1);
}
size_t Cache::get_tag(size_t addr) const
{
    return addr >> m_tag_shift; 
}

size_t Cache::form_addr(size_t tag, size_t set_index) const
{
    return tag << m_tag_shift | set_index << m_block_shift;
}

CacheNonBlocking::CacheNonBlocking(const CacheConfig &config, size_t mshr_n, std::unique_ptr<CacheInterface> next_cache)
: Cache(config, std::move(next_cache)), m_mshr_n(mshr_n)
{
    m_mshr_table.resize(m_mshr_n);
}

VictimCache::VictimCache(const CacheConfig &config, size_t victim_size, std::unique_ptr<CacheInterface> next_cache)
: Cache(config, std::move(next_cache)), m_victim_size(victim_size)
{
    m_victim_table.resize(m_victim_size);
}

size_t Cache::single_read(size_t set_index, size_t tag, size_t len)
{
    assert(len <= config.block_size);
    increment_read_count();
    auto &list = m_cache_table[set_index];
    auto it = look_up(list, tag);
    assert(config.evict_policy == EvictPolicy::LRU && "Other eviction policies are not implemented");
    if (it != list.end())
    {
        // Hit 
        CacheEntry entry = *it;
        list.erase(it);
        list.push_front(entry);
        return config.hit_latency;
    }
    else
    {
        // Miss
        increment_read_miss_count();
        size_t extra_latency = m_next_cache->config.bus_latency + m_next_cache->read(form_addr(tag, set_index), config.block_size);
        CacheEntry entry{tag, false, true};
        extra_latency += evict(set_index, entry);
        return config.hit_latency + extra_latency;
    }
}

size_t Cache::single_write(size_t set_index, size_t tag, size_t len)
{
    assert(len <= config.block_size);
    increment_write_count();
    auto &list = m_cache_table[set_index];
    auto it = look_up(list, tag);
    assert(config.evict_policy == EvictPolicy::LRU && "Other eviction policies are not implemented");
    assert(m_next_cache != nullptr);
    if (it != list.end())
    {
        // Hit
        CacheEntry entry = *it;
        size_t extra_latency = 0;
        switch (config.write_policy)
        {
        case WritePolicy::WriteThrough:
            entry.dirty = false;
            extra_latency = m_next_cache->config.bus_latency + m_next_cache->write(form_addr(tag, set_index), len);
            break;
        case WritePolicy::WriteBack:
            entry.dirty = true;
            break;
        default:
            assert(false && "Invalid write policy");
            break;
        };
        list.erase(it);
        list.push_front(entry);
        return config.hit_latency + extra_latency;
    }
    else
    {
        increment_write_miss_count();
        // Miss, write allocate
        size_t extra_latency = config.write_policy == WritePolicy::WriteThrough && config.write_miss_policy == WriteMissPolicy::NoWriteAllocate
                                   ? 0: m_next_cache->config.bus_latency + m_next_cache->read(form_addr(tag, set_index), config.block_size);
        CacheEntry entry{tag, false, true};
        switch (config.write_policy)
        {
        case WritePolicy::WriteThrough:
            entry.dirty = false;
            extra_latency += m_next_cache->config.bus_latency + m_next_cache->write(form_addr(tag, set_index), len);
            break;
        case WritePolicy::WriteBack:
            entry.dirty = true;
            break;
        default:
            assert(false && "Invalid write policy");
            break;
        };
        switch (config.write_miss_policy)
        {
        case WriteMissPolicy::WriteAllocate:
            extra_latency += evict(set_index, entry);
            break;
        case WriteMissPolicy::NoWriteAllocate:
            break;
        default:
            assert(false && "Invalid write miss policy");
            break;
        }
        return config.hit_latency + extra_latency;
    }
}

void CacheNextPretch::print_extra_info() const
{
    std::cout << "Prefetch count: " << total_prefetch_count << std::endl;
    std::cout << "Prefetch cover count: " << prefetch_cover_count << std::endl;
    std::cout << "Prefetch hit count: " << prefetch_hit_count << std::endl;
    std::cout << "Coverage Rate: " << (double)prefetch_cover_count / (get_read_count() + get_write_count()) << std::endl;
    std::cout << "Prefetch Accuracy: " << (double)prefetch_hit_count / total_prefetch_count << std::endl;
}

size_t CacheNextPretch::single_read(size_t set_index, size_t tag, size_t len)
{
    assert(len <= config.block_size);
    increment_read_count();
    auto &list = m_cache_table[set_index];
    auto it = look_up(list, tag);
    size_t latency = 0;
    bool miss = false;
    assert(config.evict_policy == EvictPolicy::LRU && "Other eviction policies are not implemented");
    if (it != list.end())
    {
        // Hit 
        CacheEntry entry = *it;
        switch (entry.prefetch)
        {
            case PrefetchStatus::NotPrefetch:
            break;
            case PrefetchStatus::Prefetch:
                prefetch_cover_count++;
                prefetch_hit_count++;
                entry.prefetch = PrefetchStatus::HitPrefetch;
            break;
            case PrefetchStatus::HitPrefetch:
                prefetch_cover_count++;
        }
        list.erase(it);
        list.push_front(entry);
        latency = config.hit_latency;
    }
    else
    {
        // Miss
        miss = true;
        increment_read_miss_count();
        size_t extra_latency = m_next_cache->config.bus_latency + m_next_cache->read(form_addr(tag, set_index), config.block_size);
        CacheEntry entry{tag, false, true};
        extra_latency += evict(set_index, entry);
        latency = config.hit_latency + extra_latency;
    }

    if (miss)
    {
        // Miss Prefetch
        this->prefetch(form_addr(tag, set_index) + config.block_size, config.block_size * m_prefetch_size);
    }
    return latency;
}

void CacheNextPretch::single_prefetch(size_t set_index, size_t tag, size_t len)
{
    assert(len <= config.block_size);
    auto &list = m_cache_table[set_index];
    auto it = look_up(list, tag);
    assert(config.evict_policy == EvictPolicy::LRU && "Other eviction policies are not implemented");
    if (it != list.end())
    {
        // Hit 
        return;
    }
    else
    {
        // Miss
        total_prefetch_count++;
        size_t extra_latency = m_next_cache->config.bus_latency + m_next_cache->read(form_addr(tag, set_index), config.block_size);
        CacheEntry entry{tag, false, true, PrefetchStatus::Prefetch};
        extra_latency += evict(set_index, entry);
        return;
    }
}

void CacheNextPretch::prefetch(size_t addr, size_t len) {
    len += addr % config.block_size;
    addr = addr - addr % config.block_size;
    while (len > 0)
    {
        size_t set_index = get_set_index(addr);
        size_t tag = get_tag(addr);
        size_t single_len = std::min(len, config.block_size);
        single_prefetch(set_index, tag, single_len);
        len -= single_len;
        addr += single_len;
    }
    assert(get_read_count() >= get_read_miss_count());
    return;
}

size_t CacheNextPretch::single_write(size_t set_index, size_t tag, size_t len)
{
    assert(len <= config.block_size);
    increment_write_count();
    auto &list = m_cache_table[set_index];
    auto it = look_up(list, tag);
    size_t latency = 0;
    bool miss = false;
    assert(config.evict_policy == EvictPolicy::LRU && "Other eviction policies are not implemented");
    assert(m_next_cache != nullptr);
    if (it != list.end())
    {
        // Hit
        CacheEntry entry = *it;
        switch (entry.prefetch)
        {
            case PrefetchStatus::NotPrefetch:
            break;
            case PrefetchStatus::Prefetch:
                prefetch_cover_count++;
                prefetch_hit_count++;
                entry.prefetch = PrefetchStatus::HitPrefetch;
            break;
            case PrefetchStatus::HitPrefetch:
                prefetch_cover_count++;
        }
        size_t extra_latency = 0;
        switch (config.write_policy)
        {
        case WritePolicy::WriteThrough:
            entry.dirty = false;
            extra_latency = m_next_cache->config.bus_latency + m_next_cache->write(form_addr(tag, set_index), len);
            break;
        case WritePolicy::WriteBack:
            entry.dirty = true;
            break;
        default:
            assert(false && "Invalid write policy");
            break;
        };
        list.erase(it);
        list.push_front(entry);
        latency = config.hit_latency + extra_latency;
    }
    else
    {
        increment_write_miss_count();
        miss = true;
        // Miss, write allocate
        size_t extra_latency = config.write_policy == WritePolicy::WriteThrough && config.write_miss_policy == WriteMissPolicy::NoWriteAllocate
                                   ? 0: m_next_cache->config.bus_latency + m_next_cache->read(form_addr(tag, set_index), config.block_size);
        CacheEntry entry{tag, false, true};
        switch (config.write_policy)
        {
        case WritePolicy::WriteThrough:
            entry.dirty = false;
            extra_latency += m_next_cache->config.bus_latency + m_next_cache->write(form_addr(tag, set_index), len);
            break;
        case WritePolicy::WriteBack:
            entry.dirty = true;
            break;
        default:
            assert(false && "Invalid write policy");
            break;
        };
        switch (config.write_miss_policy)
        {
        case WriteMissPolicy::WriteAllocate:
            extra_latency += evict(set_index, entry);
            break;
        case WriteMissPolicy::NoWriteAllocate:
            break;
        default:
            assert(false && "Invalid write miss policy");
            break;
        }
        latency = config.hit_latency + extra_latency;
    }

    if (miss)
    {
        // Miss Prefetch
        this->prefetch(form_addr(tag, set_index) + config.block_size, config.block_size * m_prefetch_size);
    }
    return latency;
}

size_t Cache::evict(size_t set_index, CacheEntry &entry)
{
    assert(config.evict_policy == EvictPolicy::LRU && "Other eviction policies are not implemented");
    auto &list = m_cache_table[set_index];
    increment_replacement_count();
    if (!list.back().valid || !list.back().dirty)
    {
        list.pop_back();
        list.push_front(entry);
        return 0;
    }
    else
    {
        size_t next_latency = m_next_cache->write(form_addr(list.back().tag, set_index), config.block_size);
        list.pop_back();
        list.push_front(entry);
        return next_latency + m_next_cache->config.bus_latency;
    }
}

std::list<CacheEntry>::iterator Cache::look_up(std::list<CacheEntry> &list, size_t tag)
{
    for (auto it = list.begin(); it != list.end(); ++it)
    {
        if (it->tag == tag && it->valid)
        {
            return it;
        }
    }
    return list.end();
}



void init_cache(int cache_sim) 
{
    if (backend_cache)
    {
        delete backend_cache;
        backend_cache = nullptr;
    }

    if (cache_sim)
    {
        CacheConfig Dram_config{0, 0, 0, EvictPolicy::LRU, WritePolicy::WriteBack, WriteMissPolicy::WriteAllocate, 200, 20};
        CacheConfig LLC_config{1 << 23, 64, 8, EvictPolicy::LRU, WritePolicy::WriteBack, WriteMissPolicy::WriteAllocate, 20, 20};
        CacheConfig L2_config{1 << 18, 64, 8, EvictPolicy::LRU, WritePolicy::WriteBack, WriteMissPolicy::WriteAllocate, 8, 6};
        CacheConfig L1_config{1 << 15, 64, 8, EvictPolicy::LRU, WritePolicy::WriteBack, WriteMissPolicy::WriteAllocate, 4, 4};
        std::unique_ptr<CacheInterface> dram = std::make_unique<Memory>(Dram_config);
        std::unique_ptr<CacheInterface> llc = std::make_unique<Cache>(LLC_config, std::move(dram));
        std::unique_ptr<CacheInterface> l2 = std::make_unique<Cache>(L2_config, std::move(llc));
        backend_cache = new Cache(L1_config, std::move(l2));
    }
    else
    {
        backend_cache = new Memory({0, 0, 0, EvictPolicy::LRU, WritePolicy::WriteBack, WriteMissPolicy::WriteAllocate, 1, 0});
    }
}

size_t write_cache(size_t addr, size_t len)
{
    assert(backend_cache != nullptr && "Cache is not initialized");
    return backend_cache->write(addr, len);
}

size_t read_cache(size_t addr, size_t len)
{
    assert(backend_cache != nullptr && "Cache is not initialized");
    return backend_cache->read(addr, len);
}

int is_cache_enabled()
{
    return dynamic_cast<Cache *>(backend_cache) != nullptr;
}

double get_L1_miss_rate()
{
    Cache *mem = dynamic_cast<Cache *>(backend_cache);
    assert(mem != nullptr && "Cache is not initialized");
    return mem->get_miss_rate();
}

double get_L2_miss_rate()
{
    Cache *mem = dynamic_cast<Cache *>(backend_cache);
    assert(mem != nullptr && "Cache is not initialized");
    mem = dynamic_cast<Cache *>(mem->get_next_cache());
    assert(mem != nullptr && "Cache is not initialized");
    return mem->get_miss_rate();
}

double get_LLC_miss_rate()
{
    Cache *mem = dynamic_cast<Cache *>(backend_cache);
    assert(mem != nullptr && "Cache is not initialized");
    mem = dynamic_cast<Cache *>(mem->get_next_cache());
    assert(mem != nullptr && "Cache is not initialized");
    mem = dynamic_cast<Cache *>(mem->get_next_cache());
    assert(mem != nullptr && "Cache is not initialized");
    return mem->get_miss_rate();
}

CacheInterface *CacheInterface::get_next_cache() const
{
    return m_next_cache.get();
}

size_t CacheSridePrefetch::read(size_t addr, size_t len)
{
    static size_t last_addr = 0;
    static bool prefetch = true;
    auto latency = Cache::read(addr, len);
    if (prefetch && latency > config.hit_latency)
    {
        // Miss Prefetch
        long stride = addr - last_addr;
        for (size_t i = 1; i <= m_prefetch_size; ++i)
        {
            this->prefetch(addr + stride * i, config.block_size);
        }
    }
    last_addr = addr;
    return latency;
}

size_t CacheSridePrefetch::write(size_t addr, size_t len)
{
    static size_t last_addr = 0;
    static bool prefetch = true;
    auto latency = Cache::write(addr, len);
    if (prefetch && latency > config.hit_latency)
    {
        // Miss Prefetch
        long stride = addr - last_addr;
        for (size_t i = 1; i <= m_prefetch_size; ++i)
        {
            this->prefetch(addr + stride * i, 1);
        }
    }
    last_addr = addr;
    return latency;
}

size_t CacheNonBlocking::single_read(size_t set_index, size_t tag, size_t len)
{
    assert(len <= config.block_size);
    increment_read_count();
    auto &list = m_cache_table[set_index];
    auto it = look_up(list, tag);
    assert(config.evict_policy == EvictPolicy::LRU && "Other eviction policies are not implemented");
    if (int cycle = conflict_cycle(form_addr(tag, set_index)); cycle > 0)
    {
        return config.hit_latency + cycle;
    }
    if (it != list.end())
    {
        // Hit 
        CacheEntry entry = *it;
        list.erase(it);
        list.push_front(entry);
        return config.hit_latency;
    }
    else
    {
        // Miss
        increment_read_miss_count();
        size_t extra_latency = m_next_cache->config.bus_latency + m_next_cache->read(form_addr(tag, set_index), config.block_size);
        CacheEntry entry{tag, false, true};
        extra_latency += evict(set_index, entry);
        if (MSHR *mshr = find_available_mshr(); mshr) {
            mshr->valid = true;
            mshr->addr = form_addr(tag, set_index);
            mshr->left_clock = config.hit_latency + extra_latency;
            extra_latency = 0;
            m_hit_under_miss++;
        }
        return config.hit_latency + extra_latency;
    }
}

size_t CacheNonBlocking::single_write(size_t set_index, size_t tag, size_t len)
{
    assert(len <= config.block_size);
    increment_write_count();
    auto &list = m_cache_table[set_index];
    auto it = look_up(list, tag);
    assert(config.evict_policy == EvictPolicy::LRU && "Other eviction policies are not implemented");
    assert(m_next_cache != nullptr);
    if (int cycle = conflict_cycle(form_addr(tag, set_index)); cycle > 0)
    {
        return config.hit_latency + cycle;
    }
    if (it != list.end())
    {
        // Hit
        CacheEntry entry = *it;
        size_t extra_latency = 0;
        switch (config.write_policy)
        {
        case WritePolicy::WriteThrough:
            entry.dirty = false;
            extra_latency = m_next_cache->config.bus_latency + m_next_cache->write(form_addr(tag, set_index), len);
            break;
        case WritePolicy::WriteBack:
            entry.dirty = true;
            break;
        default:
            assert(false && "Invalid write policy");
            break;
        };
        list.erase(it);
        list.push_front(entry);
        return config.hit_latency + extra_latency;
    }
    else
    {
        increment_write_miss_count();
        // Miss, write allocate
        size_t extra_latency = config.write_policy == WritePolicy::WriteThrough && config.write_miss_policy == WriteMissPolicy::NoWriteAllocate
                                   ? 0: m_next_cache->config.bus_latency + m_next_cache->read(form_addr(tag, set_index), config.block_size);
        CacheEntry entry{tag, false, true};
        switch (config.write_policy)
        {
        case WritePolicy::WriteThrough:
            entry.dirty = false;
            extra_latency += m_next_cache->config.bus_latency + m_next_cache->write(form_addr(tag, set_index), len);
            break;
        case WritePolicy::WriteBack:
            entry.dirty = true;
            break;
        default:
            assert(false && "Invalid write policy");
            break;
        };
        switch (config.write_miss_policy)
        {
        case WriteMissPolicy::WriteAllocate:
            extra_latency += evict(set_index, entry);
            break;
        case WriteMissPolicy::NoWriteAllocate:
            break;
        default:
            assert(false && "Invalid write miss policy");
            break;
        }
        if (MSHR *mshr = find_available_mshr(); mshr) {
            mshr->valid = true;
            mshr->addr = form_addr(tag, set_index);
            mshr->left_clock = config.hit_latency + extra_latency;
            extra_latency = 0;
            m_hit_under_miss++;       
        }
        return config.hit_latency + extra_latency;
    }
}

void CacheNonBlocking::next_clock(size_t cycles)
{
    for(auto &mshr : m_mshr_table)
    {
        if (mshr.valid)
        {
            mshr.left_clock -= (int)cycles;
            if (mshr.left_clock <= 0)
            {
                mshr.valid = false;
                mshr.left_clock = 0;
            }
        }
    }
    Cache::next_clock(cycles);
}

CacheNonBlocking::MSHR *CacheNonBlocking::find_available_mshr()
{
    for (auto &mshr : m_mshr_table)
    {
        if (!mshr.valid)
        {
            return &mshr;
        }
    }
    return nullptr;
}

int CacheNonBlocking::conflict_cycle(size_t addr)
{
    for (auto &mshr : m_mshr_table)
    {
        if (mshr.valid && mshr.addr == addr)
        {
            return mshr.left_clock;
        }
    }
    return 0;
}


size_t VictimCache::single_read(size_t set_index, size_t tag, size_t len)
{
    assert(len <= config.block_size);
    increment_read_count();
    auto &list = m_cache_table[set_index];
    auto it = look_up(list, tag, form_addr(tag, set_index));
    assert(config.evict_policy == EvictPolicy::LRU && "Other eviction policies are not implemented");
    if (it != list.end())
    {
        // Hit 
        CacheEntry entry = *it;
        list.erase(it);
        list.push_front(entry);
        return config.hit_latency;
    }
    else
    {
        // Miss
        increment_read_miss_count();
        size_t extra_latency = m_next_cache->config.bus_latency + m_next_cache->read(form_addr(tag, set_index), config.block_size);
        CacheEntry entry{tag, false, true};
        extra_latency += evict(set_index, entry);
        return config.hit_latency + extra_latency;
    }
}
size_t VictimCache::single_write(size_t set_index, size_t tag, size_t len)
{
    assert(len <= config.block_size);
    increment_write_count();
    auto &list = m_cache_table[set_index];
    auto it = look_up(list, tag, form_addr(tag, set_index));
    assert(config.evict_policy == EvictPolicy::LRU && "Other eviction policies are not implemented");
    assert(m_next_cache != nullptr);
    if (it != list.end())
    {
        // Hit
        CacheEntry entry = *it;
        size_t extra_latency = 0;
        switch (config.write_policy)
        {
        case WritePolicy::WriteThrough:
            entry.dirty = false;
            extra_latency = m_next_cache->config.bus_latency + m_next_cache->write(form_addr(tag, set_index), len);
            break;
        case WritePolicy::WriteBack:
            entry.dirty = true;
            break;
        default:
            assert(false && "Invalid write policy");
            break;
        };
        list.erase(it);
        list.push_front(entry);
        return config.hit_latency + extra_latency;
    }
    else
    {
        increment_write_miss_count();
        // Miss, write allocate
        size_t extra_latency = config.write_policy == WritePolicy::WriteThrough && config.write_miss_policy == WriteMissPolicy::NoWriteAllocate
                                   ? 0: m_next_cache->config.bus_latency + m_next_cache->read(form_addr(tag, set_index), config.block_size);
        CacheEntry entry{tag, false, true};
        switch (config.write_policy)
        {
        case WritePolicy::WriteThrough:
            entry.dirty = false;
            extra_latency += m_next_cache->config.bus_latency + m_next_cache->write(form_addr(tag, set_index), len);
            break;
        case WritePolicy::WriteBack:
            entry.dirty = true;
            break;
        default:
            assert(false && "Invalid write policy");
            break;
        };
        switch (config.write_miss_policy)
        {
        case WriteMissPolicy::WriteAllocate:
            extra_latency += evict(set_index, entry);
            break;
        case WriteMissPolicy::NoWriteAllocate:
            break;
        default:
            assert(false && "Invalid write miss policy");
            break;
        }
        return config.hit_latency + extra_latency;
    }
}
size_t VictimCache::evict(size_t set_index, CacheEntry &entry)
{
    assert(config.evict_policy == EvictPolicy::LRU && "Other eviction policies are not implemented");
    auto &list = m_cache_table[set_index];
    increment_replacement_count();
    CacheEntry back = list.back();
    list.pop_back();
    list.push_front(entry);
    m_victim_table.push_front
    (VictimCacheEntry{form_addr(entry.tag, set_index), entry.dirty, true});
    auto evicted = m_victim_table.back();
    m_victim_table.pop_back();
    if (evicted.valid && evicted.dirty)
    {
        size_t next_latency = m_next_cache->write(evicted.addr, config.block_size);
        return next_latency + m_next_cache->config.bus_latency;
    }
    else
    {
        return 0;
    }
}

std::list<CacheEntry>::iterator VictimCache::look_up(std::list<CacheEntry> &list, size_t tag, size_t addr)
{
    if (auto it = Cache::look_up(list, tag); it != list.end())
    {
        return it;
    }
    for (auto it = m_victim_table.begin(); it != m_victim_table.end(); ++it)
    {
        if (it->addr == addr && it->valid)
        {
            m_victim_hit_count++;
            CacheEntry entry{get_tag(addr), it->dirty, true};
            list.push_front(entry);
            m_victim_table.erase(it);
            auto back = list.back();
            list.pop_back();
            m_victim_table.push_front(VictimCacheEntry{form_addr(back.tag, get_set_index(addr)), back.dirty, true});
            return list.begin();
        }
    }
    return list.end();
}

void CacheSridePrefetch::print_extra_info() const
{
    std::cout << "Prefetch count: " << total_prefetch_count << std::endl;
    std::cout << "Prefetch cover count: " << prefetch_cover_count << std::endl;
    std::cout << "Prefetch hit count: " << prefetch_hit_count << std::endl;
    std::cout << "Coverage Rate: " << (double)prefetch_cover_count / (get_read_count() + get_write_count()) << std::endl;
    std::cout << "Prefetch Accuracy: " << (double)prefetch_hit_count / total_prefetch_count << std::endl;
}

size_t CacheSridePrefetch::single_read(size_t set_index, size_t tag, size_t len)
{
    assert(len <= config.block_size);
    increment_read_count();
    auto &list = m_cache_table[set_index];
    auto it = look_up(list, tag);
    size_t latency = 0;
    bool miss = false;
    assert(config.evict_policy == EvictPolicy::LRU && "Other eviction policies are not implemented");
    if (it != list.end())
    {
        // Hit 
        CacheEntry entry = *it;
        switch (entry.prefetch)
        {
            case PrefetchStatus::NotPrefetch:
            break;
            case PrefetchStatus::Prefetch:
                prefetch_cover_count++;
                prefetch_hit_count++;
                entry.prefetch = PrefetchStatus::HitPrefetch;
            break;
            case PrefetchStatus::HitPrefetch:
                prefetch_cover_count++;
        }
        list.erase(it);
        list.push_front(entry);
        latency = config.hit_latency;
    }
    else
    {
        // Miss
        miss = true;
        increment_read_miss_count();
        size_t extra_latency = m_next_cache->config.bus_latency + m_next_cache->read(form_addr(tag, set_index), config.block_size);
        CacheEntry entry{tag, false, true};
        extra_latency += evict(set_index, entry);
        latency = config.hit_latency + extra_latency;
    }
    return latency;
}

size_t CacheSridePrefetch::single_write(size_t set_index, size_t tag, size_t len)
{
        assert(len <= config.block_size);
    increment_write_count();
    auto &list = m_cache_table[set_index];
    auto it = look_up(list, tag);
    size_t latency = 0;
    bool miss = false;
    assert(config.evict_policy == EvictPolicy::LRU && "Other eviction policies are not implemented");
    assert(m_next_cache != nullptr);
    if (it != list.end())
    {
        // Hit
        CacheEntry entry = *it;
        switch (entry.prefetch)
        {
            case PrefetchStatus::NotPrefetch:
            break;
            case PrefetchStatus::Prefetch:
                prefetch_cover_count++;
                prefetch_hit_count++;
                entry.prefetch = PrefetchStatus::HitPrefetch;
            break;
            case PrefetchStatus::HitPrefetch:
                prefetch_cover_count++;
        }
        size_t extra_latency = 0;
        switch (config.write_policy)
        {
        case WritePolicy::WriteThrough:
            entry.dirty = false;
            extra_latency = m_next_cache->config.bus_latency + m_next_cache->write(form_addr(tag, set_index), len);
            break;
        case WritePolicy::WriteBack:
            entry.dirty = true;
            break;
        default:
            assert(false && "Invalid write policy");
            break;
        };
        list.erase(it);
        list.push_front(entry);
        latency = config.hit_latency + extra_latency;
    }
    else
    {
        increment_write_miss_count();
        miss = true;
        // Miss, write allocate
        size_t extra_latency = config.write_policy == WritePolicy::WriteThrough && config.write_miss_policy == WriteMissPolicy::NoWriteAllocate
                                   ? 0: m_next_cache->config.bus_latency + m_next_cache->read(form_addr(tag, set_index), config.block_size);
        CacheEntry entry{tag, false, true};
        switch (config.write_policy)
        {
        case WritePolicy::WriteThrough:
            entry.dirty = false;
            extra_latency += m_next_cache->config.bus_latency + m_next_cache->write(form_addr(tag, set_index), len);
            break;
        case WritePolicy::WriteBack:
            entry.dirty = true;
            break;
        default:
            assert(false && "Invalid write policy");
            break;
        };
        switch (config.write_miss_policy)
        {
        case WriteMissPolicy::WriteAllocate:
            extra_latency += evict(set_index, entry);
            break;
        case WriteMissPolicy::NoWriteAllocate:
            break;
        default:
            assert(false && "Invalid write miss policy");
            break;
        }
        latency = config.hit_latency + extra_latency;
    }
    return latency;
}

void CacheSridePrefetch::prefetch(size_t addr, size_t len) {
    len += addr % config.block_size;
    addr = addr - addr % config.block_size;
    while (len > 0)
    {
        size_t set_index = get_set_index(addr);
        size_t tag = get_tag(addr);
        size_t single_len = std::min(len, config.block_size);
        single_prefetch(set_index, tag, single_len);
        len -= single_len;
        addr += single_len;
    }
    assert(get_read_count() >= get_read_miss_count());
    return;
}

void CacheSridePrefetch::single_prefetch(size_t set_index, size_t tag, size_t len)
{
    assert(len <= config.block_size);
    auto &list = m_cache_table[set_index];
    auto it = look_up(list, tag);
    assert(config.evict_policy == EvictPolicy::LRU && "Other eviction policies are not implemented");
    if (it != list.end())
    {
        // Hit 
        return;
    }
    else
    {
        // Miss
        total_prefetch_count++;
        size_t extra_latency = m_next_cache->config.bus_latency + m_next_cache->read(form_addr(tag, set_index), config.block_size);
        CacheEntry entry{tag, false, true, PrefetchStatus::Prefetch};
        extra_latency += evict(set_index, entry);
        return;
    }
}

CacheFinal::CacheFinal(const CacheConfig &config, size_t mshr_n, size_t prefetch_size, std::unique_ptr<CacheInterface> next_cache)
: Cache(config, std::move(next_cache)), CacheNextPretch(config, prefetch_size, nullptr)
, CacheNonBlocking(config, mshr_n, nullptr) {}

size_t CacheFinal::single_read(size_t set_index, size_t tag, size_t len)
{
    assert(len <= config.block_size);
    increment_read_count();
    auto &list = m_cache_table[set_index];
    auto it = look_up(list, tag);
    size_t latency = 0;
    bool miss = false;
    assert(config.evict_policy == EvictPolicy::LRU && "Other eviction policies are not implemented");
    if (int cycle = conflict_cycle(form_addr(tag, set_index)); cycle > 0)
    {
        return config.hit_latency + cycle;
    }
    if (it != list.end())
    {
        // Hit 
        CacheEntry entry = *it;
        switch (entry.prefetch)
        {
            case PrefetchStatus::NotPrefetch:
            break;
            case PrefetchStatus::Prefetch:
                prefetch_cover_count++;
                prefetch_hit_count++;
                entry.prefetch = PrefetchStatus::HitPrefetch;
            break;
            case PrefetchStatus::HitPrefetch:
                prefetch_cover_count++;
        }
        list.erase(it);
        list.push_front(entry);
        latency = config.hit_latency;
    }
    else
    {
        // Miss
        miss = true;
        increment_read_miss_count();
        size_t extra_latency = m_next_cache->config.bus_latency + m_next_cache->read(form_addr(tag, set_index), config.block_size);
        CacheEntry entry{tag, false, true};
        extra_latency += evict(set_index, entry);
        if (MSHR *mshr = find_available_mshr(); mshr) {
            mshr->valid = true;
            mshr->addr = form_addr(tag, set_index);
            mshr->left_clock = config.hit_latency + extra_latency;
            extra_latency = 0;
            m_hit_under_miss++;       
        }
        latency = config.hit_latency + extra_latency;
    }

    if (miss)
    {
        // Miss Prefetch
        this->prefetch(form_addr(tag, set_index) + config.block_size, config.block_size * m_prefetch_size);
    }
    return latency;
}

size_t CacheFinal::single_write(size_t set_index, size_t tag, size_t len)
{
    assert(len <= config.block_size);
    increment_write_count();
    auto &list = m_cache_table[set_index];
    auto it = look_up(list, tag);
    size_t latency = 0;
    bool miss = false;
    assert(config.evict_policy == EvictPolicy::LRU && "Other eviction policies are not implemented");
    assert(m_next_cache != nullptr);
    if (int cycle = conflict_cycle(form_addr(tag, set_index)); cycle > 0)
    {
        return config.hit_latency + cycle;
    }
    if (it != list.end())
    {
        // Hit
        CacheEntry entry = *it;
        switch (entry.prefetch)
        {
            case PrefetchStatus::NotPrefetch:
            break;
            case PrefetchStatus::Prefetch:
                prefetch_cover_count++;
                prefetch_hit_count++;
                entry.prefetch = PrefetchStatus::HitPrefetch;
            break;
            case PrefetchStatus::HitPrefetch:
                prefetch_cover_count++;
        }
        size_t extra_latency = 0;
        switch (config.write_policy)
        {
        case WritePolicy::WriteThrough:
            entry.dirty = false;
            extra_latency = m_next_cache->config.bus_latency + m_next_cache->write(form_addr(tag, set_index), len);
            break;
        case WritePolicy::WriteBack:
            entry.dirty = true;
            break;
        default:
            assert(false && "Invalid write policy");
            break;
        };
        list.erase(it);
        list.push_front(entry);
        latency = config.hit_latency + extra_latency;
    }
    else
    {
        increment_write_miss_count();
        miss = true;
        // Miss, write allocate
        size_t extra_latency = config.write_policy == WritePolicy::WriteThrough && config.write_miss_policy == WriteMissPolicy::NoWriteAllocate
                                   ? 0: m_next_cache->config.bus_latency + m_next_cache->read(form_addr(tag, set_index), config.block_size);
        CacheEntry entry{tag, false, true};
        switch (config.write_policy)
        {
        case WritePolicy::WriteThrough:
            entry.dirty = false;
            extra_latency += m_next_cache->config.bus_latency + m_next_cache->write(form_addr(tag, set_index), len);
            break;
        case WritePolicy::WriteBack:
            entry.dirty = true;
            break;
        default:
            assert(false && "Invalid write policy");
            break;
        };
        switch (config.write_miss_policy)
        {
        case WriteMissPolicy::WriteAllocate:
            extra_latency += evict(set_index, entry);
            break;
        case WriteMissPolicy::NoWriteAllocate:
            break;
        default:
            assert(false && "Invalid write miss policy");
            break;
        }

        if (MSHR *mshr = find_available_mshr(); mshr) {
            mshr->valid = true;
            mshr->addr = form_addr(tag, set_index);
            mshr->left_clock = config.hit_latency + extra_latency;
            extra_latency = 0;
            m_hit_under_miss++;       
        }

        latency = config.hit_latency + extra_latency;
    }

    if (miss)
    {
        // Miss Prefetch
        this->prefetch(form_addr(tag, set_index) + config.block_size, config.block_size * m_prefetch_size);
    }
    return latency;
}
