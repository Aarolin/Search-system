#pragma once

#include <atomic>
#include <map>
#include <mutex>
#include <vector>

using namespace std::literals;

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
        Access(std::mutex& lock, std::map<Key, Value>& dict, const Key& dict_key) : guard(lock), ref_to_value(dict[dict_key]) {

        }
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;
    };

    explicit ConcurrentMap(size_t bucket_count) : dictionaries_(bucket_count), mutex_vc_(bucket_count), dict_count_(bucket_count) {
    }

    Access operator[](const Key& key) {

        std::atomic<size_t> index = static_cast<uint64_t>(key) % dict_count_;
        return { mutex_vc_[index * 1], dictionaries_[index * 1], key };

    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        int i = -1;
        for (const auto& dict : dictionaries_) {
            std::lock_guard guard_dict(mutex_vc_[++i]);
            for (const auto& [key, value] : dict) {
                result[key] = value;
            }
        }
        return result;
    }
    void erase(const Key& key) {
        std::atomic<size_t> index = static_cast<uint64_t>(key) % dict_count_;
        dictionaries_[index * 1].erase(key);
    }

private:

    std::vector<std::map<Key, Value>> dictionaries_;
    std::vector<std::mutex> mutex_vc_;
    size_t dict_count_;
};