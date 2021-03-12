// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_COMMON_FLAT_MAP_H_
#define STARBOARD_COMMON_FLAT_MAP_H_

#include "starboard/common/log.h"
#include "starboard/types.h"

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

namespace starboard {
namespace flat_map_detail {
// IsPod<> is a white list of common types that are "plain-old-data'.
// Types not registered with IsPod<> will default to non-pod.
// Usage: IsPod<int>::value == true;
//        IsPod<std::string>::value == false
// See specializations at the bottom of this file for what has been defined
// as pod.
template <typename T>
struct IsPod {
  enum { value = false };
};
}  // namespace flat_map_detail.

// FlatMap<key_type, mapped_type> is a sorted vector map of key-value pairs.
// This map has an interface that is largely compatible with std::map<> and
// is designed to be a near drop in replacement for std::map<>.
//
// A main usage difference between FlatMap<> and std::map<> is that FlatMap<>
// will invalidate it's iterators on insert() and erase().
//
// Reasons to use this map include low-level operations which require that
// the map does not allocate memory during runtime (this is achievable by
// invoking FlatMap::reserve()), or to speed up a lot of find() operations
// where the FlatMap() is mutated to very occasionally.
//
// PERFORMANCE
//   where n is the number of items in flatmap
//   and m is the input size for the operation.
//
// bulk insert |  O(m*log(m) + m*log(n) + n+m) (sort input, check dups, merge)
// insert      |  O(n)
// erase       |  O(n)
// bulk erase  |  O(n*m)     TODO: Make faster - O(n+m + log(m))
// find        |  O(log(n))
// clear       |  O(n)
//
// Performance of FlatMap::find() tends to be about +50% faster than that
// of std::map<> for pod types in optimized builds. However this is balanced
// by the fact that FlatMap::insert() and FlatMap::erase() both operate at
// O(n), where std::map will operate at O(log n) for both operations.
//
// FlatMap<int,int>::find() Performance
// NUMBER OF ELEMENTS | SPEED COMPARISON vs std::map
// -------------------------------------
//                  5 | 220.37%
//                 10 | 158.602%
//                 25 | 87.7049%
//                 50 | 91.0873%
//                100 | 96.1367%
//               1000 | 120.588%
//              10000 | 156.969%
//             100000 | 179.55%
//
// When in doubt, use std::map. If you need to use FlatMap<>, then make sure
// that insertion() is done in bulk, and that delete are infrequent, or that
// the maps are small.
//
// Please see unit tests for additional usage.

template <typename Key,
          typename Value,
          typename LessThan = std::less<Key>,
          typename VectorType = std::vector<std::pair<Key, Value> > >
class FlatMap {
 public:
  // Most typedefs here are to make FlatMap<> compatible with std::map<>.
  typedef Key key_type;
  typedef Value mapped_type;
  typedef LessThan key_compare;
  typedef std::pair<key_type, mapped_type> value_type;
  typedef typename VectorType::size_type size_type;
  typedef typename VectorType::difference_type difference_type;
  typedef typename VectorType::iterator iterator;
  typedef typename VectorType::const_iterator const_iterator;

  FlatMap() {}
  FlatMap(const FlatMap& other) : vector_(other.vector_) {}

  template <typename ITERATOR>
  FlatMap(ITERATOR start, ITERATOR end) {
    insert(start, end);
  }

  void reserve(size_t size) { vector_.reserve(size); }

  // Takes the incoming elements and only adds the elements that don't already
  // exist in this map.
  // Returns the number of elements that were added.
  template <typename Iterator>
  size_t insert(Iterator begin_it, Iterator end_it) {
    const size_t partition_idx = vector_.size();

    // PART 1 - Elements are added unsorted into array as a new partition.
    //
    // Only add elements that don't exist
    for (Iterator it = begin_it; it != end_it; ++it) {
      // These have to be recomputed every loop iteration because
      // vector_.push_back() will invalidate iterators.
      const_iterator sorted_begin = vector_.begin();
      const_iterator sorted_end = sorted_begin + partition_idx;

      const bool already_exists_sorted_part =
          exists_in_range(sorted_begin, sorted_end, it->first);
      // Okay so it doesn't exist yet so place it in the vector.
      if (!already_exists_sorted_part) {
        vector_.push_back(*it);
      }
    }

    // No elements added.
    if (vector_.size() == partition_idx) {
      return 0;
    }

    iterator unsorted_begin = vector_.begin() + partition_idx;
    // std::sort(...) will not maintain the order of values which have
    // keys. Therefore InplaceMergeSort(...) is used instead, which has
    // the same guarantees as std::stable_sort but with the added addition
    // that no memory will be allocated during the operation of
    // InplaceMergeSort(...). This is important because when bulk inserting
    // elements, the first key-value pair (of duplicate keys) will be the one
    // that gets inserted, and the second one is ignored.
    InplaceMergeSort(unsorted_begin, vector_.end());

    // Corner-case: remove duplicates in the input.
    iterator new_end = std::unique(unsorted_begin, vector_.end(), EqualTo);
    std::inplace_merge(vector_.begin(), unsorted_begin, new_end, LessThanValue);

    vector_.erase(new_end, vector_.end());

    // partition_idx was the previous size of the vector_.
    const size_t num_elements_added = vector_.size() - partition_idx;
    return num_elements_added;
  }

  std::pair<iterator, bool> insert(const value_type& entry) {
    iterator insertion_it =
        std::upper_bound(begin(), end(), entry, LessThanValue);

    // DUPLICATE CHECK - If the key already exists then return it.
    if (insertion_it != begin()) {
      // Check for a duplicate value, which will be the preceding value, if
      // it exits.
      iterator previous_it = (insertion_it - 1);
      const key_type& previous_key = previous_it->first;
      if (EqualKeyTo(previous_key, entry.first)) {
        // Value already exists.
        return std::pair<iterator, bool>(previous_it, false);
      }
    }

    iterator inserted_it = vector_.insert(insertion_it, entry);
    return std::pair<iterator, bool>(inserted_it, true);
  }

  iterator find(const key_type& key) {
    return find_in_range(vector_.begin(), vector_.end(), key);
  }

  const_iterator find(const key_type& key) const {
    return find_in_range_const(vector_.begin(), vector_.end(), key);
  }

  bool erase(const key_type& key) {
    iterator found_it = find(key);
    if (found_it != vector_.end()) {
      vector_.erase(found_it);  // no resorting necessary.
      return true;
    } else {
      return false;
    }
  }

  void erase(iterator it) { vector_.erase(it); }

  void erase(iterator begin_it, iterator end_it) {
    vector_.erase(begin_it, end_it);
  }

  bool empty() const { return vector_.empty(); }
  size_t size() const { return vector_.size(); }
  iterator begin() { return vector_.begin(); }
  const_iterator begin() const { return vector_.begin(); }
  const_iterator cbegin() const { return vector_.begin(); }
  iterator end() { return vector_.end(); }
  const_iterator end() const { return vector_.end(); }
  const_iterator cend() const { return vector_.end(); }
  void clear() { vector_.clear(); }

  size_t count(const key_type& key) const {
    if (cend() != find(key)) {
      return 1;
    } else {
      return 0;
    }
  }

  size_type max_size() const { return vector_.max_size(); }
  void swap(FlatMap& other) { vector_.swap(other.vector_); }

  iterator lower_bound(const key_type& key) {
    mapped_type dummy;
    value_type key_data(key, dummy);  // Second value is ignored.
    return std::lower_bound(begin(), end(), key_data, LessThanValue);
  }

  const_iterator lower_bound(const key_type& key) const {
    mapped_type dummy;
    value_type key_data(key, dummy);  // Second value is ignored.
    return std::lower_bound(cbegin(), cend(), key_data, LessThanValue);
  }

  iterator upper_bound(const key_type& key) {
    mapped_type dummy;
    value_type key_data(key, dummy);  // Second value is ignored.
    return std::upper_bound(begin(), end(), key_data, LessThanValue);
  }

  const_iterator upper_bound(const key_type& key) const {
    mapped_type dummy;
    value_type key_data(key, dummy);  // Second value is ignored.
    return std::upper_bound(cbegin(), cend(), key_data, LessThanValue);
  }

  std::pair<iterator, iterator> equal_range(const key_type& key) {
    iterator found = find(key);
    if (found == end()) {
      return std::pair<iterator, iterator>(end(), end());
    }
    iterator found_end = found;
    ++found_end;
    return std::pair<iterator, iterator>(found, found_end);
  }

  std::pair<const_iterator, const_iterator> equal_range(
      const key_type& key) const {
    const_iterator found = find(key);
    if (found == end()) {
      return std::pair<const_iterator, const_iterator>(cend(), cend());
    }
    const_iterator found_end = found;
    ++found_end;
    return std::pair<const_iterator, const_iterator>(found, found_end);
  }

  key_compare key_comp() const {
    key_compare return_value;
    return return_value;
  }

  mapped_type& operator[](const key_type& key) {
    std::pair<key_type, mapped_type> entry;
    entry.first = key;
    std::pair<iterator, bool> result = insert(entry);
    iterator found = result.first;
    mapped_type& value = found->second;
    return value;
  }

  bool operator==(const FlatMap& other) const {
    return vector_ == other.vector_;
  }

  bool operator!=(const FlatMap& other) const {
    return vector_ != other.vector_;
  }

 private:
  static bool LessThanValue(const std::pair<key_type, mapped_type>& a,
                            const std::pair<key_type, mapped_type>& b) {
    return LessThanKey(a.first, b.first);
  }

  static bool LessThanKey(const key_type& a, const key_type& b) {
    key_compare less_than;
    return less_than(a, b);
  }

  static bool NotEqualKeyTo(const key_type& a, const key_type& b) {
    key_compare less_than;
    return less_than(a, b) || less_than(b, a);
  }

  static bool EqualKeyTo(const key_type& a, const key_type& b) {
    return !NotEqualKeyTo(a, b);
  }

  static bool EqualTo(const std::pair<key_type, mapped_type>& a,
                      const std::pair<key_type, mapped_type>& b) {
    return EqualKeyTo(a.first, b.first);
  }

  static iterator find_in_range(iterator begin_it,
                                iterator end_it,
                                const key_type& key) {
    // Delegate to find_in_range_const().
    const_iterator begin_it_const = begin_it;
    const_iterator end_it_const = end_it;
    const_iterator found_it_const =
        find_in_range_const(begin_it_const, end_it_const, key);
    const size_t diff = std::distance(begin_it_const, found_it_const);
    return begin_it + diff;
  }

  static inline const_iterator find_in_range_const_linear(
      const_iterator begin_it,
      const_iterator end_it,
      const value_type& key_data) {
    SB_DCHECK(end_it >= begin_it);
    for (const_iterator it = begin_it; it != end_it; ++it) {
      if (LessThanValue(key_data, *it)) {
        continue;
      }
      if (!LessThanValue(*it, key_data)) {
        return it;
      }
    }
    return end_it;
  }

  static inline const_iterator find_in_range_const(const_iterator begin_it,
                                                   const_iterator end_it,
                                                   const key_type& key) {
    // This was tested and found to have a very positive effect on
    // performance. The threshold could be a lot higher (~20 elements) but is
    // kept at 11 elements to be on the conservative side.
    static const difference_type kLinearSearchThreshold = 11;

    mapped_type dummy;
    value_type key_data(key, dummy);  // Second value is ignored.

    // Speedup for small maps of pod type: just do a linear search. This was
    // tested to be significantly faster - 2x speedup. The conditions for this
    // search are very specific so that in many situations the fast path won't
    // be taken.
    if (flat_map_detail::IsPod<key_type>::value) {
      const difference_type range_distance = std::distance(begin_it, end_it);

      // Linear search.
      if (range_distance < kLinearSearchThreshold) {
        return find_in_range_const_linear(begin_it, end_it, key_data);
      }
    }

    const_iterator found_it =
        std::lower_bound(begin_it, end_it, key_data, LessThanValue);
    if (found_it == end_it) {
      return end_it;
    }
    if (NotEqualKeyTo(found_it->first, key)) {  // different keys.
      return end_it;
    }
    size_t dist = std::distance(begin_it, found_it);
    return begin_it + dist;
  }

  static bool exists_in_range(const_iterator begin_it,
                              const_iterator end_it,
                              const key_type& key) {
    const_iterator result_iterator = find_in_range_const(begin_it, end_it, key);
    return result_iterator != end_it;
  }

  // This is needed as a stable sorting algorithm, which leaves duplicates in
  // relative order from which they appeared in the original container.
  // Unlike std::stable_sort(...) this function will not allocate memory.
  void InplaceMergeSort(iterator begin_it, iterator end_it) {
    if (end_it - begin_it > 1) {
      iterator middle_it = begin_it + (end_it - begin_it) / 2;
      InplaceMergeSort(begin_it, middle_it);
      InplaceMergeSort(middle_it, end_it);
      std::inplace_merge(begin_it, middle_it, end_it, LessThanValue);
    }
  }

  VectorType vector_;
};

namespace flat_map_detail {
#define STARBOARD_FLATMAP_DEFINE_IS_POD(TYPE) \
  template <>                                 \
  struct IsPod<TYPE> {                        \
    enum { value = true };                    \
  }

STARBOARD_FLATMAP_DEFINE_IS_POD(bool);
STARBOARD_FLATMAP_DEFINE_IS_POD(float);
STARBOARD_FLATMAP_DEFINE_IS_POD(double);
STARBOARD_FLATMAP_DEFINE_IS_POD(int8_t);
STARBOARD_FLATMAP_DEFINE_IS_POD(uint8_t);
STARBOARD_FLATMAP_DEFINE_IS_POD(int16_t);
STARBOARD_FLATMAP_DEFINE_IS_POD(uint16_t);
STARBOARD_FLATMAP_DEFINE_IS_POD(int32_t);
STARBOARD_FLATMAP_DEFINE_IS_POD(uint32_t);
STARBOARD_FLATMAP_DEFINE_IS_POD(int64_t);
STARBOARD_FLATMAP_DEFINE_IS_POD(uint64_t);

#undef STARBOARD_FLATMAP_DEFINE_IS_POD

// Specialization - all pointer types are treated as pod.
template <typename T>
struct IsPod<T*> {
  enum { value = true };
};

}  // namespace flat_map_detail.
}  // namespace starboard.

#endif  // STARBOARD_COMMON_FLAT_MAP_H_
