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

    explicit ConcurrentMap(size_t bucket_count) : dictionaries_(bucket_count) {
    }

    Access operator[](const Key& key) {

        std::atomic<size_t> index = static_cast<uint64_t>(key) % dictionaries_.size();
        return { dictionaries_[index].dict_mutex, dictionaries_[index].dictionary, key };

    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        for (auto& bucket : dictionaries_) {
            std::lock_guard guard_dict(bucket.dict_mutex);
            for (const auto& [key, value] : bucket.dictionary) {
                result[key] = value;
            }
        }
        return result;
    }
    void erase(const Key& key) {
        std::atomic<size_t> index = static_cast<uint64_t>(key) % dictionaries_.size();
        dictionaries_[index].dictionary.erase(key);
    }

private:

    struct Bucket {
        std::map<Key, Value> dictionary;
        std::mutex dict_mutex;
    };

    std::vector<Bucket> dictionaries_;

};