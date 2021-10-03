#ifndef AFINA_STORAGE_STRIPED_LRU_H
#define AFINA_STORAGE_STRIPED_LRU_H


#include "SimpleLRU.h"
#include <vector>

namespace Afina {
namespace Backend {

/**
 * # Map based implementation
 * That is NOT thread safe implementaiton!!
 */
class StripedLRU: public Afina::Storage {
public:


    StripedLRU(std::size_t max_size, std::size_t stripe_count);

    ~StripedLRU() = default;

    static std::unique_ptr<StripedLRU> BuildStripedLRU(std::size_t memory_limit = 1024, std::size_t stripe_count = 8);


    // Implements Afina::Storage interface
    bool Put(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool PutIfAbsent(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Set(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Delete(const std::string &key) override;

    // Implements Afina::Storage interface
    bool Get(const std::string &key, std::string &value) override;

private:
    std::size_t _capacity = 0;
    std::size_t _stripe_count = 0;
    std::hash<std::string> hash;
    std::vector<std::mutex> _mutex;
    std::vector< SimpleLRU> _shard;
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_SIMPLE_LRU_H
