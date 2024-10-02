// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_DENSE_SET_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_DENSE_SET_H_

#include <array>
#include <bit>
#include <climits>
#include <cstddef>
#include <iterator>
#include <type_traits>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/numerics/safe_conversions.h"

namespace autofill {

// A set container with a std::set<T>-like interface for integral or enum types
// T that have a dense and small representation as unsigned integers.
//
// The order of the elements in the container corresponds to their integer
// representation.
//
// The lower and upper bounds of elements storable in a container are
// [T(0), kMaxValue]. By default, kMaxValue is T::kMaxValue.
//
// The `packed` parameter indicates whether the memory consumption of a DenseSet
// object should be minimized. That comes at the cost of slightly larger code
// size.
//
// Time and space complexity:
// - insert(), erase(), contains() should run in time O(1)
// - empty(), size(), iteration run in time O(kMaxValue)
// - sizeof(DenseSet) is, for N = kMaxValue + 1,
//   - if `!packed`: the minimum of {1, 2, 4, 8 * ceil(N / 64)} bytes that has
//     at least N bits;
//   - if `packed`: ceil(N / 8) bytes.
//
// Iterators are invalidated when the owning container is destructed or moved,
// or when the element the iterator points to is erased from the container.
template <typename T, T kMaxValue = T::kMaxValue, bool packed = false>
class DenseSet {
 private:
  // The index of a bit.
  using Index = std::make_unsigned_t<T>;

  static_assert(std::is_integral<T>::value || std::is_enum<T>::value);
  static_assert(0 <= base::checked_cast<Index>(kMaxValue) + 1);

  // The maximum supported bit index. Indexing starts at 0, so kMaxBitIndex ==
  // 63 means we need 64 bits.
  static constexpr size_t kMaxBitIndex = base::checked_cast<Index>(kMaxValue);

 public:
  // The bitset is represented as array of words.
  using Word = std::conditional_t<
      !packed,
      std::conditional_t<
          (kMaxBitIndex < 8),
          uint8_t,
          std::conditional_t<
              (kMaxBitIndex < 16),
              uint16_t,
              std::conditional_t<(kMaxBitIndex < 32), uint32_t, uint64_t>>>,
      uint8_t>;

 private:
  // Returns ceil(x / y).
  static constexpr size_t ceil_div(size_t x, size_t y) {
    return (x + y - 1) / y;
  }

  static constexpr size_t kBitsPerWord = sizeof(Word) * CHAR_BIT;

 public:
  // The number of `Word`s needed to hold `kMaxBitIndex + 1` bits.
  static constexpr size_t kNumWords = ceil_div(kMaxBitIndex + 1, kBitsPerWord);

  // A bidirectional iterator for the DenseSet.
  class Iterator {
   public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = T;

    constexpr Iterator() = default;

    friend bool operator==(const Iterator& a, const Iterator& b) {
      DCHECK(a.owner_);
      DCHECK_EQ(a.owner_, b.owner_);
      return a.index_ == b.index_;
    }

    friend bool operator!=(const Iterator& a, const Iterator& b) {
      return !(a == b);
    }

    T operator*() const {
      DCHECK(derefenceable());
      return index_to_value(index_);
    }

    Iterator& operator++() {
      ++index_;
      Skip(kForward);
      return *this;
    }

    Iterator operator++(int) {
      auto that = *this;
      operator++();
      return that;
    }

    Iterator& operator--() {
      --index_;
      Skip(kBackward);
      return *this;
    }

    Iterator operator--(int) {
      auto that = *this;
      operator--();
      return that;
    }

   private:
    friend DenseSet;

    enum Direction { kBackward = -1, kForward = 1 };

    constexpr Iterator(const DenseSet* owner, Index index)
        : owner_(owner), index_(index) {}

    // Advances the index, starting from the current position, to the next
    // non-empty one.
    void Skip(Direction direction) {
      DCHECK_LE(index_, owner_->max_size());
      while (index_ < owner_->max_size() && !derefenceable()) {
        index_ += direction;
      }
    }

    bool derefenceable() const {
      DCHECK_LT(index_, owner_->max_size());
      return owner_->get_bit(index_);
    }

    // This field is not a raw_ptr<> because it was filtered by the rewriter
    // for: #constexpr-ctor-field-initializer
    RAW_PTR_EXCLUSION const DenseSet* owner_ = nullptr;

    // The current index is in the interval [0, owner_->max_size()].
    Index index_ = 0;
  };

  using value_type = T;
  using iterator = Iterator;
  using const_iterator = Iterator;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  constexpr DenseSet() = default;

  constexpr DenseSet(std::initializer_list<T> init) {
    for (const auto& x : init) {
      set_bit(value_to_index(x));
    }
  }

  template <typename InputIt>
  DenseSet(InputIt first, InputIt last) {
    for (auto it = first; it != last; ++it) {
      insert(*it);
    }
  }

  // Returns a raw bitmask. Useful for serialization.
  constexpr base::span<const Word, kNumWords> data() const { return words_; }

  friend bool operator==(const DenseSet& a, const DenseSet& b) {
    return a.words_ == b.words_;
  }

  friend bool operator!=(const DenseSet& a, const DenseSet& b) {
    return !(a == b);
  }

  // Iterators.

  // Returns an iterator to the beginning.
  iterator begin() const {
    const_iterator it(this, 0);
    it.Skip(Iterator::kForward);
    return it;
  }
  const_iterator cbegin() const { return begin(); }

  // Returns an iterator to the end.
  iterator end() const { return iterator(this, max_size()); }
  const_iterator cend() const { return end(); }

  // Returns a reverse iterator to the beginning.
  reverse_iterator rbegin() const { return reverse_iterator(end()); }
  const_reverse_iterator crbegin() const { return rbegin(); }

  // Returns a reverse iterator to the end.
  reverse_iterator rend() const { return reverse_iterator(begin()); }
  const_reverse_iterator crend() const { return rend(); }

  // Capacity.

  // Returns true if the set is empty, otherwise false.
  constexpr bool empty() const { return words_ == kOnlyZeros; }

  // Returns the number of elements the set has.
  constexpr size_t size() const {
    // We count the number of bits in `words_`. DenseSet ensures that all bits
    // beyond `kMaxBitIndex` are zero. This is necessary for size() to be
    // correct.
    DCHECK_EQ(words_.back() & (~0ULL << (kMaxBitIndex % kBitsPerWord + 1)),
              0ULL);
    size_t num_set_bits = 0;
    for (const auto word : words_) {
      num_set_bits += std::popcount(word);
    }
    return num_set_bits;
  }

  // Returns the maximum number of elements the set can have.
  constexpr size_t max_size() const { return kMaxBitIndex + 1; }

  // Modifiers.

  // Clears the contents.
  constexpr void clear() { words_ = {}; }

  // Inserts value |x| if it is not present yet, and returns an iterator to the
  // inserted or existing element and a boolean that indicates whether the
  // insertion took place.
  constexpr std::pair<iterator, bool> insert(T x) {
    bool contained = contains(x);
    set_bit(value_to_index(x));
    return {find(x), !contained};
  }

  // Inserts all values of |xs| into the present set.
  constexpr void insert_all(const DenseSet& xs) {
    DCHECK_EQ(words_.size(), xs.words_.size());
    for (size_t i = 0; i < words_.size(); ++i) {
      words_[i] |= xs.words_[i];
    }
  }

  // Erases the element whose index matches the index of |x| and returns the
  // number of erased elements (0 or 1).
  size_t erase(T x) {
    bool contained = contains(x);
    unset_bit(value_to_index(x));
    return contained ? 1 : 0;
  }

  // Erases the element |*it| and returns an iterator to its successor.
  iterator erase(const_iterator it) {
    DCHECK(it.owner_ == this && it.derefenceable());
    unset_bit(it.index_);
    it.Skip(const_iterator::kForward);
    return it;
  }

  // Erases the elements [first,last) and returns |last|.
  iterator erase(const_iterator first, const_iterator last) {
    DCHECK(first.owner_ == this && last.owner_ == this);
    while (first != last) {
      unset_bit(first.index_);
      ++first;
    }
    return last;
  }

  // Erases all values of |xs| into the present set.
  void erase_all(const DenseSet& xs) {
    DCHECK_EQ(words_.size(), xs.words_.size());
    for (size_t i = 0; i < words_.size(); ++i) {
      words_[i] &= ~xs.words_[i];
    }
  }

  // Lookup.

  // Returns 1 if |x| is an element, otherwise 0.
  constexpr size_t count(T x) const { return contains(x) ? 1 : 0; }

  // Returns an iterator to the element |x| if it exists, otherwise end().
  constexpr const_iterator find(T x) const {
    return contains(x) ? const_iterator(this, value_to_index(x)) : cend();
  }

  // Returns true if |x| is an element, else |false|.
  constexpr bool contains(T x) const { return get_bit(value_to_index(x)); }

  // Returns true if some element of |xs| is an element, else |false|.
  bool contains_none(const DenseSet& xs) const {
    return intersection(words_, xs.words_) == kOnlyZeros;
  }

  // Returns true if some element of |xs| is an element, else |false|.
  bool contains_any(const DenseSet& xs) const {
    return intersection(words_, xs.words_) != kOnlyZeros;
  }

  // Returns true if every elements of |xs| is an element, else |false|.
  bool contains_all(const DenseSet& xs) const {
    return intersection(words_, xs.words_) == xs.words_;
  }

  // Returns an iterator to the first element not less than the |x|, or end().
  const_iterator lower_bound(T x) const {
    const_iterator it(this, value_to_index(x));
    it.Skip(Iterator::kForward);
    return it;
  }

  // Returns an iterator to the first element greater than |x|, or end().
  const_iterator upper_bound(T x) const {
    const_iterator it(this, value_to_index(x) + 1);
    it.Skip(Iterator::kForward);
    return it;
  }

 private:
  friend Iterator;

  using Words = std::array<Word, kNumWords>;

  // Needed to use std::conditional_t.
  // Must be declared outside of index_to_value() to avoid compiler errors.
  struct Wrapper {
    using type = T;
  };

  static constexpr Index value_to_index(T x) {
    DCHECK(index_to_value(0) <= x && x <= kMaxValue);
    return base::checked_cast<Index>(x);
  }

  static constexpr T index_to_value(Index i) {
    DCHECK_LE(i, base::checked_cast<Index>(kMaxValue));
    using UnderlyingType =
        typename std::conditional_t<std::is_enum<T>::value,
                                    std::underlying_type<T>, Wrapper>::type;
    return static_cast<T>(base::checked_cast<UnderlyingType>(i));
  }

  static constexpr Words intersection(const Words& lhs, const Words& rhs) {
    DCHECK_EQ(lhs.size(), rhs.size());
    Words result{};
    for (size_t i = 0; i < lhs.size(); ++i) {
      result[i] = lhs[i] & rhs[i];
    }
    return result;
  }

  constexpr bool get_bit(Index index) const {
    DCHECK_LE(index, kMaxBitIndex);
    size_t word = index / kBitsPerWord;
    size_t bit = index % kBitsPerWord;
    return words_[word] & (static_cast<Word>(1) << bit);
  }

  constexpr void set_bit(Index index) {
    DCHECK_LE(index, kMaxBitIndex);
    size_t word = index / kBitsPerWord;
    size_t bit = index % kBitsPerWord;
    words_[word] |= static_cast<Word>(1) << bit;
  }

  constexpr void unset_bit(Index index) {
    DCHECK_LE(index, kMaxBitIndex);
    size_t word = index / kBitsPerWord;
    size_t bit = index % kBitsPerWord;
    words_[word] &= ~(static_cast<Word>(1) << bit);
  }

  static constexpr Words kOnlyZeros = Words{};

  Words words_{};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_DENSE_SET_H_
