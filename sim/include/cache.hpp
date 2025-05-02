#ifndef __CACHE_HPP__
#define __CACHE_HPP__
extern "C"
{
#include "cache.h"
}

#include <vector>
#include <list>
#include <tuple>
#include <memory>

enum class PrefetchStatus
{
    NotPrefetch,
    Prefetch,
    HitPrefetch,
};

/**
 * @file cache.hpp
 * @brief This file is the header file as the backend of memory module of the simulator.
 * This module does not serve as acual memory.
 * We assume the address length is the same as size_t.
 */

struct CacheEntry
{
    size_t tag = 0;
    bool dirty = false;
    bool valid = false;
    PrefetchStatus prefetch = PrefetchStatus::NotPrefetch;
};

class CacheInterface
{
public:
    CacheInterface(const CacheConfig &config) : config(config) {}
    const CacheConfig config;
    /**
     * @returns The number of cycles to get the data from the cache.
     */
    virtual size_t write(size_t addr, size_t len) = 0;

    /**
     * @returns The number of cycles to get the data from the cache.
     */
    virtual size_t read(size_t addr, size_t len) = 0;
    virtual ~CacheInterface() = default;
    virtual void print_extra_info() const {};

    size_t get_write_count() const { return m_write_count; }
    size_t get_read_count() const { return m_read_count; }
    size_t get_write_miss_count() const { return m_write_miss_count; }
    size_t get_read_miss_count() const { return m_read_miss_count; }
    size_t get_replacement_count() const { return m_replacement_count; }
    double get_miss_rate() const
    {
        return static_cast<double>(m_write_miss_count + m_read_miss_count)
         / static_cast<double>(m_write_count + m_read_count);
    }
    CacheInterface *get_next_cache() const;

protected:
    std::unique_ptr<CacheInterface> m_next_cache = nullptr;
    void increment_write_count() { if (m_is_counted) m_write_count++; }
    void increment_read_count() { if (m_is_counted) m_read_count++; }
    void increment_write_miss_count() { if (m_is_counted) m_write_miss_count++; }
    void increment_read_miss_count() { if (m_is_counted) m_read_miss_count++; }
    void increment_replacement_count() { if (m_is_counted) m_replacement_count++; }
    void set_counted(bool counted) { m_is_counted = counted; }
    bool is_counted() const { return m_is_counted; }
    
private:
    // The number of times the cache is accessed. It is the count for line cache.
    bool m_is_counted = true;
    size_t m_write_count = 0;
    size_t m_read_count = 0;
    size_t m_write_miss_count = 0;
    size_t m_read_miss_count = 0;
    size_t m_replacement_count = 0;
};

// Memory always hit, the time is hit_latency.
class Memory : public CacheInterface
{
public:
    Memory(const CacheConfig &config) : CacheInterface(config) {}
    size_t write(size_t addr, size_t len) override { increment_write_count(); return config.hit_latency; }
    size_t read(size_t addr, size_t len) override { increment_read_count(); return config.hit_latency;}
};

class Cache : public CacheInterface
{
public:
    Cache(const CacheConfig &config, std::unique_ptr<CacheInterface> next_cache = nullptr);

    size_t write(size_t addr, size_t len) override;
    size_t read(size_t addr, size_t len) override;

protected:
    /**
     * @brief m_cache_data[i][j]
     */
    std::vector<std::list<CacheEntry>> m_cache_table;
    size_t m_tag_shift;
    size_t m_block_shift;
    size_t m_set_n;
    
    size_t get_set_index(size_t addr) const;
    size_t get_tag(size_t addr) const;
    size_t form_addr(size_t tag, size_t set_index) const; 

    virtual size_t single_read(size_t set_index, size_t tag, size_t len);
    virtual size_t single_write(size_t set_index, size_t tag, size_t len); 

    /**
     * @returns The number of cycles to evict the entry.
     */
    size_t evict(size_t set_index, CacheEntry &entry);

    static std::list<CacheEntry>::iterator look_up(std::list<CacheEntry> &list, size_t tag);
};

class CacheNextPretch : public Cache
{
public:
    CacheNextPretch(const CacheConfig &config, size_t prefetch_size,
    std::unique_ptr<CacheInterface> next_cache = nullptr) : Cache(config, std::move(next_cache)), m_prefetch_size(prefetch_size) {}
    void print_extra_info() const override;
protected:
    size_t m_prefetch_size;
    size_t total_prefetch_count = 0;
    size_t prefetch_cover_count = 0;
    size_t prefetch_hit_count = 0; 
    size_t single_read(size_t set_index, size_t tag, size_t len) override;
    void single_prefetch(size_t set_index, size_t tag, size_t len);
    void prefetch(size_t addr, size_t len);
    size_t single_write(size_t set_index, size_t tag, size_t len) override;
    
};

class CacheSridePrefetch : public Cache
{
public:
    CacheSridePrefetch(const CacheConfig &config, size_t prefetch_size, 
    std::unique_ptr<CacheInterface> next_cache = nullptr
    ) : Cache(config, std::move(next_cache)), m_prefetch_size(prefetch_size) {}
protected:
    size_t m_prefetch_size;
    size_t read(size_t addr, size_t len) override;
    size_t write(size_t addr, size_t len) override;
};

#endif