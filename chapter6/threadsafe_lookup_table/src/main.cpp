#include <algorithm>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

template <typename Key, typename Value, typename Hash = std::hash<Key>>
class threadsafe_lookup_table {
 private:
  class bucket_type {
   private:
    using bucket_value = std::pair<Key, Value>;
    using bucket_data = std::list<bucket_value>;
    using bucket_iterator = typename bucket_data::iterator;
    bucket_data data;
    mutable std::shared_mutex mut;
    bucket_iterator find_entry_for(const Key& k) {
      return std::find_if(
          data.begin(), data.end(),
          [&](const bucket_value& item) { return item.first == k; });
    }

   public:
    Value value_for(const Key& k, const Value& default_value) const {
      std::shared_lock<std::shared_mutex> lock(mut);
      bucket_iterator found_entry = find_entry_for(k);
      return (found_entry == data.end()) ? default_value : found_entry->second;
    }
    void add_or_update_mapping(const Key& k, const Value& v) {
      std::unique_lock<std::shared_mutex> lock(mut);
      bucket_iterator found_entry = find_entry_for(k);
      if (found_entry == data.end()) {
        data.push_back(bucket_value(k, v));
      } else {
        found_entry->second = v;
      }
    }
    void remove_mapping(const Key& k) {
      std::unique_lock<std::shared_mutex> lock(mut);
      bucket_iterator found_entry = find_entry_for(k);
      if (found_entry != data.end()) {
        data.erase(found_entry);
      }
    }
  };

  std::vector<std::unique_ptr<bucket_type>> buckets;
  Hash hasher;
  bucket_type& get_bucket(const Key& k) const {
    std::size_t const index = hasher(k) % buckets.size();
    return *buckets[index];
  }

 public:
  threadsafe_lookup_table(unsigned num_buckets = 19, const Hash& h = Hash())
      : buckets(num_buckets), hasher(h) {
    for (unsigned i = 0; i < num_buckets; ++i) {
      buckets[i].reset(new bucket_type);
    }
  }
  threadsafe_lookup_table(const threadsafe_lookup_table& other) = delete;
  threadsafe_lookup_table& operator=(const threadsafe_lookup_table& other) =
      delete;

  Value value_for(const Key& k, const Value& default_value) const {
    return get_bucket(k).value_for(k, default_value);
  }
  void add_or_update_mapping(const Key& k, const Value& v) {
    get_bucket(k).add_or_update_mapping(k, v);
  }
  void remove_mapping(const Key& k) { get_bucket(k).remove_mapping(k); }
  std::map<Key, Value> get_map() const {
    std::vector<std::unique_lock<std::shared_mutex>> locks;
    for (unsigned i = 0; i < buckets.size(); ++i) {
      locks.emplace_back(std::unique_lock<std::shared_mutex>(buckets[i]->mut));
    }
    std::map<Key, Value> res;
    for (unsigned i = 0; i < buckets.size(); ++i) {
      for (auto it = buckets[i]->data.begin(); it != buckets[i]->data.end();
           ++it) {
        res.insert(*it);
      }
    }
    return res;
  }
};