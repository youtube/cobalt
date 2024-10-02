// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAIL_CC_SCOPED_PTR_EXPIRING_CACHE_H_
#define CHROME_BROWSER_THUMBNAIL_CC_SCOPED_PTR_EXPIRING_CACHE_H_

#include <stddef.h>

#include <memory>

#include "net/third_party/quiche/src/quiche/common/quiche_linked_hash_map.h"

namespace thumbnail {

template <class Key, class Value>
class ScopedPtrExpiringCache {
 private:
  typedef quiche::QuicheLinkedHashMap<Key, Value*> LinkedHashMap;

 public:
  typedef typename LinkedHashMap::iterator iterator;

  explicit ScopedPtrExpiringCache(size_t max_cache_size)
      : max_cache_size_(max_cache_size) {}

  ScopedPtrExpiringCache(const ScopedPtrExpiringCache&) = delete;
  ScopedPtrExpiringCache& operator=(const ScopedPtrExpiringCache&) = delete;

  ~ScopedPtrExpiringCache() {}

  void Put(const Key& key, std::unique_ptr<Value> value) {
    Remove(key);
    map_[key] = value.release();
    EvictIfFull();
  }

  Value* Get(const Key& key) {
    iterator iter = map_.find(key);
    if (iter != map_.end()) {
      return iter->second;
    }
    return nullptr;
  }

  std::unique_ptr<Value> Remove(const Key& key) {
    iterator iter = map_.find(key);
    std::unique_ptr<Value> value;
    if (iter != map_.end()) {
      value.reset(iter->second);
      map_.erase(key);
    }
    return std::move(value);
  }

  void Clear() {
    for (iterator iter = map_.begin(); iter != map_.end(); iter++) {
      delete iter->second;
    }
    map_.clear();
  }

  iterator begin() { return map_.begin(); }
  iterator end() { return map_.end(); }
  size_t MaximumCacheSize() const { return max_cache_size_; }
  size_t size() const { return map_.size(); }

 private:
  void EvictIfFull() {
    while (map_.size() > max_cache_size_) {
      iterator it = map_.begin();
      delete it->second;
      map_.erase(it);
    }
  }

  size_t max_cache_size_;
  LinkedHashMap map_;
};

}  // namespace thumbnail

#endif  // CHROME_BROWSER_THUMBNAIL_CC_SCOPED_PTR_EXPIRING_CACHE_H_
