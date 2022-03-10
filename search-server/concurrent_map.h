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

  explicit ConcurrentMap(size_t bucket_count)
    : maps_(bucket_count), v_m_(bucket_count) {};

  Access operator[](const Key& key) {
    auto index = GetIndex(key);

    return {std::lock_guard(v_m_[index]), maps_[index][key]};
  };

  std::map<Key, Value> BuildOrdinaryMap() {
    std::map<Key, Value> result;

    for (unsigned long int i = 0; i < maps_.size(); ++i) {
      std::lock_guard guard(v_m_[i]);
      result.merge(maps_[i]);
    }

    return result;
  };

  void Erase(const Key &key) {
    auto index = GetIndex(key);

    std::lock_guard guard(v_m_[index]);
    maps_[index].erase(key);
  }

private:
  std::vector<std::map<Key, Value>> maps_;
  std::vector<std::mutex> v_m_;

  uint64_t GetIndex(const Key &key) {
    return key % maps_.size();
  }
};
