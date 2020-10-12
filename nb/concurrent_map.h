/*
 * Copyright 2017 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NB_CONCURRENT_MAP_H_
#define NB_CONCURRENT_MAP_H_

#include <iterator>
#include <map>
#include <vector>

#include "nb/hash.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"

namespace nb {

template <typename KeyT,
          typename ValueT,
          typename HashFunction,
          typename InnerMap = std::map<KeyT, ValueT> >
class ConcurrentMap;

// Map of int -> int
typedef ConcurrentMap<int, int, PODHasher<int> > IntToIntConcurrentMap;

// Map of void* -> void*
typedef ConcurrentMap<const void*, void*, PODHasher<const void*> >
    PtrToPtrConcurrentMap;

// ConcurrentMap<> is a well performing map which maintains better performance
// for multiple thread access than an std::map with a single mutex.
//
// Concurrent access is achieved by partitioning the key-space into k buckets.
// An input key is randomly assigned to one of the buckets by a hash function.
// The selected bucket then contains it's own mutex and map, which is then
// accessed.
//
// Contention:
//  Example:
//    If there are 32 buckets, then two threads both executing
//    Set(key,value) will have a ~3% chance of contention (1/32).
//    For four threads the chances are ~40%.
template <typename KeyT,
          typename ValueT,
          typename HashFunction,
          typename InnerMap>
class ConcurrentMap {
 public:
  // EntryHandle and ConstEntryHandle provides mutually exclusive access to
  // key-values access in this map. These handles will lock the entire bucket
  // and it's recommended that these handles are released as soon as possible.
  // The handles are automatically released when the object goes out of scope
  // or is deleted.
  class EntryHandle;
  class ConstEntryHandle;
  struct Bucket;

  // |num_concurrency_buckets| specifies the number of concurrency buckets
  // that will be created. The higher the number of buckets, the lower the
  // probability of thread contention. 511 was experimentally found to be a
  // good value maximizing performance on a highly contended pointer map.
  // Lowering the number of buckets will reduce memory consumption.
  ConcurrentMap(size_t num_concurrency_buckets = 511) {
    if (num_concurrency_buckets < 1) {
      SB_DCHECK(false);  // Should reach here.
      num_concurrency_buckets = 1;
    }
    bucket_table_.resize(num_concurrency_buckets);
  }

  // Returns true if object was set to the new value. Otherwise false means
  // that the object already existed.
  bool SetIfMissing(const KeyT& key, const ValueT& value) {
    Bucket& bucket = GetBucketForKey(key);
    starboard::ScopedLock lock(bucket.mutex_);
    bool inserted = bucket.map_.insert(std::make_pair(key, value)).second;
    return inserted;
  }

  // Sets the object. Whatever value was previously there is replaced.
  void Set(const KeyT& key, const ValueT& value) {
    EntryHandle entry_handle;
    GetOrCreate(key, &entry_handle);
    entry_handle.ValueMutable() = value;
  }

  // Gets the value using the key. If the value does not already exist then
  // a default value is created. The resulting value is stored in the
  // EntryHandle.
  void GetOrCreate(const KeyT& key, EntryHandle* entry_handle) {
    ValueT default_val = ValueT();
    Bucket& bucket = GetBucketForKey(key);
    entry_handle->AcquireLock(&bucket);
    typename InnerMap::iterator it =
        bucket.map_.insert(std::make_pair(key, default_val)).first;
    entry_handle->ValidateIterator(it);
  }

  // Returns true if a value exists for the input key.
  bool Has(const KeyT& key) const {
    const Bucket& bucket = GetBucketForKey(key);
    starboard::ScopedLock lock(bucket.mutex_);

    typename InnerMap::const_iterator found_it = bucket.map_.find(key);
    return found_it != bucket.map_.end();
  }

  // Gets the value from the key. The value in the map can be accessed from the
  // EntryHandle. The EntryHandle will hold onto an internal lock for the
  // bucket which contains the key-value pair. It's recommended that Release()
  // is invoked on the entry_handle as soon as caller is done accessing the
  // value to release the lock and reduce potential contention.
  // Returns |true| if a key-value pair existed in the map and |false|
  // otherwise. When returning |true| |entry_handle| will be set to a valid
  // value will maintain a lock on the bucket until Release() is invoked or the
  // object goes out of scope. If |false| is returned then the entry_handle
  // be released() and no lock will maintain after the call completes.
  bool Get(const KeyT& key, EntryHandle* entry_handle) {
    Bucket& bucket = GetBucketForKey(key);
    entry_handle->AcquireLock(&bucket);

    typename InnerMap::iterator found_it = bucket.map_.find(key);
    if (found_it == bucket.map_.end()) {
      entry_handle->ReleaseLockAndInvalidate();
      return false;
    }
    entry_handle->ValidateIterator(found_it);
    return true;
  }

  // See mutable version of Get().
  bool Get(const KeyT& key, ConstEntryHandle* entry_handle) const {
    const Bucket& bucket = GetBucketForKey(key);
    entry_handle->AcquireLock(&bucket);

    typename InnerMap::const_iterator found_it = bucket.map_.find(key);
    if (found_it == bucket.map_.end()) {
      entry_handle->ReleaseLockAndInvalidate();
      return false;
    }
    entry_handle->ValidateIterator(found_it);
    return true;
  }

  // Removes the key-value mapping from this map. Returns |true| if the mapping
  // existed, else |false|.
  bool Remove(const KeyT& key) {
    Bucket& bucket = GetBucketForKey(key);
    starboard::ScopedLock lock(bucket.mutex_);
    typename InnerMap::iterator found_it = bucket.map_.find(key);
    const bool exists = (found_it != bucket.map_.end());
    if (exists) {
      bucket.map_.erase(found_it);
    }
    return exists;
  }

  bool Remove(EntryHandle* entry_handle) {
    if (!entry_handle->Valid()) {
      SB_DCHECK(false);  // Can't remove an invalid entry_handle!
      return false;
    }

    Bucket* bucket = entry_handle->bucket_;
    bucket->map_.erase(entry_handle->iterator_);
    entry_handle->ReleaseLockAndInvalidate();
    return true;
  }

  size_t GetSize() const {
    typedef typename std::vector<Bucket>::const_iterator table_iterator;

    size_t sum = 0;
    for (table_iterator it = bucket_table_.begin(); it != bucket_table_.end();
         ++it) {
      const Bucket& bucket = *it;
      bucket.mutex_.Acquire();
      sum += bucket.map_.size();
      bucket.mutex_.Release();
    }
    return sum;
  }

  size_t IsEmpty() const { return !GetSize(); }

  void Clear() {
    typedef typename std::vector<Bucket>::iterator table_iterator;

    for (table_iterator it = bucket_table_.begin(); it != bucket_table_.end();
         ++it) {
      Bucket& bucket = *it;
      bucket.mutex_.Acquire();
      bucket.map_.clear();
      bucket.mutex_.Release();
    }
  }

  // The supplied visitor will accept every key-value pair in the concurrent
  // map. This traversal will visit the elements in an undefined order. A
  // warning about performance: while traversing elements, a bucket lock will
  // be maintained and if the visitor is expensive then it's possible that this
  // could cause other threads to block.
  template <typename KeyValueVisitorT>
  void ForEach(KeyValueVisitorT* visitor) {
    typedef typename std::vector<Bucket>::iterator bucket_iterator;
    typedef typename InnerMap::iterator map_iterator;

    for (bucket_iterator it = bucket_table_.begin(); it != bucket_table_.end();
         ++it) {
      Bucket& bucket = *it;
      bucket.mutex_.Acquire();
      for (map_iterator it = bucket.map_.begin(); it != bucket.map_.end();
           ++it) {
        visitor->Visit(it->first, it->second);
      }
      bucket.mutex_.Release();
    }
  }

  // Under the circumstances that an operation can be applied to a copy of
  // the internal data, then it's advised to copy this data to an std::vector
  // and then apply the operation to the data.
  void CopyToStdVector(std::vector<std::pair<KeyT, ValueT> >* destination) {
    typedef typename std::vector<Bucket>::iterator bucket_iterator;
    typedef typename InnerMap::iterator map_iterator;

    for (bucket_iterator it = bucket_table_.begin(); it != bucket_table_.end();
         ++it) {
      Bucket& bucket = *it;
      bucket.mutex_.Acquire();
      destination->insert(destination->end(), bucket.map_.begin(),
                          bucket.map_.end());
      bucket.mutex_.Release();
    }
  }

  class EntryHandle {
   public:
    friend class ConcurrentMap;

    EntryHandle() : bucket_(NULL), iterator_(), iterator_valid_(false) {}
    ~EntryHandle() { ReleaseLockAndInvalidate(); }

    const ValueT& Value() {
      SB_DCHECK(iterator_valid_);
      return iterator_->second;
    }

    ValueT& ValueMutable() {
      SB_DCHECK(iterator_valid_);
      return iterator_->second;
    }

    const KeyT& Key() {
      SB_DCHECK(iterator_valid_);
      return iterator_->first;
    }

    bool Valid() const { return iterator_valid_; }

    void ReleaseLockAndInvalidate() {
      ReleaseLockIfNecessary();
      iterator_valid_ = false;
    }

   private:
    void ValidateIterator(typename InnerMap::iterator it) {
      iterator_ = it;
      iterator_valid_ = true;
    }

    void AcquireLock(Bucket* bucket) {
      ReleaseLockIfNecessary();
      bucket_ = bucket;
      bucket_->mutex_.Acquire();
    }

    void ReleaseLockIfNecessary() {
      if (bucket_) {
        bucket_->mutex_.Release();
      }
      bucket_ = NULL;
    }

    Bucket* bucket_;
    typename InnerMap::iterator iterator_;
    bool iterator_valid_;

    EntryHandle(const EntryHandle&) = delete;
    void operator=(const EntryHandle&) = delete;
  };

  class ConstEntryHandle {
   public:
    friend class ConcurrentMap;

    ConstEntryHandle() : bucket_(NULL), iterator_(), iterator_valid_(false) {}
    ~ConstEntryHandle() { ReleaseLockAndInvalidate(); }

    const ValueT& Value() {
      SB_DCHECK(iterator_valid_);
      return iterator_->second;
    }

    const KeyT& Key() {
      SB_DCHECK(iterator_valid_);
      return iterator_->first;
    }

    bool Valid() const { return iterator_valid_; }

    void ReleaseLockAndInvalidate() {
      ReleaseLockIfNecessary();
      iterator_valid_ = false;
    }

   private:
    void ValidateIterator(typename InnerMap::const_iterator it) {
      iterator_ = it;
      iterator_valid_ = true;
    }

    void AcquireLock(const Bucket* bucket) {
      ReleaseLockIfNecessary();
      bucket_ = bucket;
      bucket_->mutex_.Acquire();
    }

    void ReleaseLockIfNecessary() {
      if (bucket_) {
        bucket_->mutex_.Release();
      }
      bucket_ = NULL;
    }

    const Bucket* bucket_;
    typename InnerMap::const_iterator iterator_;
    bool iterator_valid_;

    ConstEntryHandle(const ConstEntryHandle&) = delete;
    void operator=(const ConstEntryHandle&) = delete;
  };

  struct Bucket {
    Bucket() {}
    Bucket(const Bucket& b) { map_ = b.map_; }
    void operator=(const Bucket& b) { map_ = b.map_; }
    InnerMap map_;
    mutable starboard::Mutex mutex_;
  };

 private:
  Bucket& GetBucketForKey(const KeyT& key) {
    size_t table_index = hasher_(key) % bucket_table_.size();
    return bucket_table_[table_index];
  }

  const Bucket& GetBucketForKey(const KeyT& key) const {
    size_t table_index = hasher_(key) % bucket_table_.size();
    return bucket_table_[table_index];
  }

  std::vector<Bucket> bucket_table_;
  HashFunction hasher_;
};

}  // namespace nb

#endif  // NB_CONCURRENT_MAP_H_
