#include "StripedLRU.h"

namespace Afina {
namespace Backend {

StripedLRU::StripedLRU(std::size_t memory_limit,
                       std::size_t stripe_count) :
                                                    _capacity(memory_limit / stripe_count),
                                                   _stripe_count(stripe_count),
                                                    _mutex(stripe_count) {
    for (std::size_t i = 0 ; i < _stripe_count; i++) {
        _shard.emplace_back(SimpleLRU(_capacity));
    }
}

bool StripedLRU::Put(const std::string &key, const std::string &value) {
    std::lock_guard<std::mutex> _lock(_mutex[hash(key) % _stripe_count]);
    return _shard[hash(key) % _stripe_count].Put(key, value);
}

bool StripedLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    std::lock_guard<std::mutex> _lock(_mutex[hash(key) % _stripe_count]);
    return _shard[hash(key) % _stripe_count].PutIfAbsent(key, value);
}

bool StripedLRU::Set(const std::string &key, const std::string &value) {
    std::lock_guard<std::mutex> _lock(_mutex[hash(key) % _stripe_count]);
    return _shard[hash(key) % _stripe_count].Set(key, value);
}

bool StripedLRU::Delete(const std::string &key) {
    std::lock_guard<std::mutex> _lock(_mutex[hash(key) % _stripe_count]);
    return _shard[hash(key) % _stripe_count].Delete(key);
}

bool StripedLRU::Get(const std::string &key, std::string &value) {
    std::lock_guard<std::mutex> _lock(_mutex[hash(key) % _stripe_count]);
    return _shard[hash(key) % _stripe_count].Get(key, value);
}

std::unique_ptr<StripedLRU> StripedLRU::BuildStripedLRU(std::size_t memory_limit, std::size_t stripe_count) {
    std::size_t stripe_limit = memory_limit / stripe_count;
    if (stripe_count != 0) {
        stripe_limit = memory_limit / stripe_count;
    } else {
        throw std::runtime_error("Wrong stripe count");
    }
    if (stripe_limit < 1024 * 1024UL) {
        throw std::runtime_error("Storage size too small");
    }
    StripedLRU* new_lru = new StripedLRU(memory_limit, stripe_count);

    return std::unique_ptr<StripedLRU>(new_lru);
}

}

} // namespace Afina