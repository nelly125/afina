#ifndef AFINA_STORAGE_THREAD_SAFE_SIMPLE_LRU_H
#define AFINA_STORAGE_THREAD_SAFE_SIMPLE_LRU_H

#include <map>
#include <mutex>
#include <string>

#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

/**
 * # SimpleLRU thread safe version
 *
 *
 */
class ThreadSafeSimpleLRU : public SimpleLRU {
public:
    ThreadSafeSimpleLRU(size_t max_size = 1024) : SimpleLRU(max_size) {}
    ~ThreadSafeSimpleLRU() {}

    ThreadSafeSimpleLRU(ThreadSafeSimpleLRU&&) = default;


    // see SimpleLRU.h
    bool Put(const std::string &key, const std::string &value) override {
        std::lock_guard<std::mutex> _lock(_mutex);
        return SimpleLRU::Put(key, value);
    }

    // see SimpleLRU.h
    bool PutIfAbsent(const std::string &key, const std::string &value) override {
        std::lock_guard<std::mutex> _lock(_mutex);
        return SimpleLRU::PutIfAbsent(key, value);
    }

    // see SimpleLRU.h
    bool Set(const std::string &key, const std::string &value) override {
        std::lock_guard<std::mutex> _lock(_mutex);
        return SimpleLRU::Set(key, value);
    }

    // see SimpleLRU.h
    bool Delete(const std::string &key) override {
        std::lock_guard<std::mutex> _lock(_mutex);
        return SimpleLRU::Delete(key);
    }

    // see SimpleLRU.h
    bool Get(const std::string &key, std::string &value) override {
        std::lock_guard<std::mutex> _lock(_mutex);
        return SimpleLRU::Get(key, value);
    }

private:
    std::mutex _mutex;
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_THREAD_SAFE_SIMPLE_LRU_H
