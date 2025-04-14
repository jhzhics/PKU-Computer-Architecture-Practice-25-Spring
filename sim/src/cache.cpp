#include "cache.hpp"
#include <bitset>
#include <cassert>
#include <algorithm>
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
    assert(m_write_count >= m_write_miss_count);
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
    assert(m_read_count >= m_read_miss_count);
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

size_t Cache::single_read(size_t set_index, size_t tag, size_t len)
{
    assert(len <= config.block_size);
    m_read_count++;
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
        m_read_miss_count++;
        size_t extra_latency = m_next_cache->config.bus_latency + m_next_cache->read(form_addr(tag, set_index), config.block_size);
        CacheEntry entry{tag, false, true};
        extra_latency += evict(set_index, entry);
        return config.hit_latency + extra_latency;
    }
}

size_t Cache::single_write(size_t set_index, size_t tag, size_t len)
{
    assert(len <= config.block_size);
    m_write_count++;
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
        m_write_miss_count++;
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

size_t Cache::evict(size_t set_index, CacheEntry &entry)
{
    assert(config.evict_policy == EvictPolicy::LRU && "Other eviction policies are not implemented");
    auto &list = m_cache_table[set_index];
    m_replacement_count++;
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
