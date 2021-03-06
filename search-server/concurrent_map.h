#pragma once

#include <map>
#include <mutex>
#include <vector>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
public:
  static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

  struct Access {
    std::lock_guard<std::mutex> guard;
    Value &ref_to_value;
  };

  explicit ConcurrentMap(size_t bucket_count) : maps_(bucket_count) {};

  Access operator[](const Key& key) {
    auto index = GetIndex(key);

    return {std::lock_guard(maps_[index].mutex), maps_[index].map[key]};
  };

  std::map<Key, Value> BuildOrdinaryMap() {
    std::map<Key, Value> result;

    for (size_t i = 0; i < maps_.size(); ++i) {
      std::lock_guard guard(maps_[i].mutex);
      result.merge(maps_[i].map);
    }

    return result;
  };

  void Erase(const Key &key) {
    auto index = GetIndex(key);

    std::lock_guard guard(maps_[index].mutex);
    maps_[index].map.erase(key);
  }

private:
  struct LockedMap {
    std::map<Key, Value> map;
    std::mutex mutex;
  };

  std::vector<LockedMap> maps_;

  uint64_t GetIndex(const Key &key) {
    return key % maps_.size();
  }
};
