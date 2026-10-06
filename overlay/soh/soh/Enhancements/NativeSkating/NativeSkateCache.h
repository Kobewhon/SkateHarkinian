#pragma once
#include <map>
#include <cstddef>
#include <cstdint>
namespace NativeSkateCache {
template <class Key, class Value> class Bounded {
    struct Entry {
        Value value;
        size_t bytes;
        uint64_t stamp;
    };
    std::map<Key, Entry> entries;
    uint64_t clock = 0;
    size_t used = 0, limit, countLimit;

  public:
    uint64_t hits = 0, misses = 0, evictions = 0;
    explicit Bounded(size_t count = 8, size_t bytes = 16 * 1024 * 1024) : limit(bytes), countLimit(count) {
    }
    const Value* Find(const Key& key) {
        auto i = entries.find(key);
        if (i == entries.end()) {
            ++misses;
            return nullptr;
        }
        ++hits;
        i->second.stamp = ++clock;
        return &i->second.value;
    }
    void Put(const Key& key, const Value& value, size_t bytes) {
        auto i = entries.find(key);
        if (i != entries.end()) {
            used -= i->second.bytes;
            entries.erase(i);
        }
        if (bytes > limit)
            return;
        while (!entries.empty() && (entries.size() >= countLimit || used + bytes > limit)) {
            auto old = entries.begin();
            for (auto j = entries.begin(); j != entries.end(); ++j)
                if (j->second.stamp < old->second.stamp)
                    old = j;
            used -= old->second.bytes;
            entries.erase(old);
            ++evictions;
        }
        entries.emplace(key, Entry{ value, bytes, ++clock });
        used += bytes;
    }
    void Clear() {
        entries.clear();
        used = 0;
    }
    size_t Count() const {
        return entries.size();
    }
    size_t Bytes() const {
        return used;
    }
};
} // namespace NativeSkateCache
