// Copyright 2016 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef COBALT_BASE_FIXED_SIZE_LRU_CACHE_H_
#define COBALT_BASE_FIXED_SIZE_LRU_CACHE_H_

#include <algorithm>
#include <functional>

#include "base/basictypes.h"
#include "base/logging.h"

namespace base {

// This is an LRU cache that uses an array with an LRU eviction policy.
// This class takes owner ship of which ever value you put in it.
// When the item is evicted from the cache, a Deleter will be called
// on each object.  Deleter should be a functor that implements the operator().
template <typename Key, typename T, std::size_t kMaxItemsInBuffer,
          typename Deleter, typename KeyCompare = std::equal_to<Key> >
class FixedSizeLRUCache {
 public:
  struct value_type {
    Key key;
    T value;
  };

  typedef value_type* iterator;
  typedef value_type const* const_iterator;

  // Functor that checks if two items have keys that match (based on a template
  // argument).
  struct KeyEqual : public std::unary_function<KeyEqual, bool> {
    explicit KeyEqual(const Key& key) : to_check(key) {}
    bool operator()(const value_type& i) const {
      KeyCompare comparator;
      return comparator(i.key, to_check);
    }
    Key to_check;
  };

  iterator begin() { return data_; }
  const_iterator begin() const { return data_; }
  iterator end() {
    DCHECK_LE(current_index_, kMaxItemsInBuffer);
    return begin() + current_index_;
  }
  const_iterator end() const {
    DCHECK_LE(current_index_, kMaxItemsInBuffer);
    return begin() + current_index_;
  }

  std::size_t size() const { return std::distance(begin(), end()); }

  FixedSizeLRUCache() : current_index_(0) {}
  ~FixedSizeLRUCache() {
    for (iterator it = begin(); it != end(); ++it) {
      item_deleter_(it->value);
    }
  }

  // Complexity is O(N).
  iterator find(const Key& key) {
    value_type* array_end = end();
    iterator it = std::find_if(begin(), array_end, KeyEqual(key));
    if (it != array_end) {
      MoveToFront(it);

      return begin();
    }

    return array_end;
  }

  bool empty() const { return current_index_ == 0; }

  // Return true if an item was evicted.
  bool put(const Key& key, const T& value) {
    value_type* array_end = end();
    value_type* it = std::find_if(begin(), array_end, KeyEqual(key));

    bool something_was_evicted(false);

    if (it == array_end) {
      if (current_index_ == kMaxItemsInBuffer) {
        COMPILE_ASSERT(kMaxItemsInBuffer > 0, array_size_must_be_non_zero);
        something_was_evicted = true;
        item_deleter_(data_[kMaxItemsInBuffer - 1].value);
      } else {
        DCHECK_LT(current_index_, kMaxItemsInBuffer);
        ++current_index_;
      }

      ShiftValuesDown(begin(), end());

      value_type& item(*begin());
      item.key = key;
      item.value = value;
    } else {
      value_type& item(*it);
      something_was_evicted = true;
      item_deleter_(item.value);
      item.value = value;
    }

    return something_was_evicted;
  }

 private:
  value_type data_[kMaxItemsInBuffer];
  std::size_t current_index_;
  Deleter item_deleter_;

  void MoveToFront(iterator it) {
    if (it == begin()) {
      return;
    }
    DCHECK_NE(it, end());
    value_type tmp(*it);
    ShiftValuesDown(begin(), it + 1);
    std::swap(*begin(), tmp);
    COMPILE_ASSERT(kMaxItemsInBuffer > 0, array_size_must_be_non_zero);
  }

  // This function takes in two forward iterators, and shifts items down
  // so that the starting_iterator now points to an empty value that can be
  // written to, and the value near the ending_iterator is overwritten.
  // The function does not call the destructor on any values, so it is advised
  // that the destructor be called on the value that is to be overwritten before
  // this function is called.
  void ShiftValuesDown(iterator starting_iterator, iterator ending_iterator) {
    std::size_t distance = std::distance(starting_iterator, ending_iterator);
    if (distance > 1) {
      std::size_t items_to_move = distance - 1;
      DCHECK_LT(items_to_move, kMaxItemsInBuffer);
      std::copy_backward(starting_iterator, starting_iterator + items_to_move,
                         ending_iterator);
    }
  }
};

}  // namespace base

#endif  // COBALT_BASE_FIXED_SIZE_LRU_CACHE_H_
