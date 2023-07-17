// Copyright 2017 Google Inc. All Rights Reserved.
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
//

#ifndef NB_REWINDABLE_VECTOR_CAST_H_
#define NB_REWINDABLE_VECTOR_CAST_H_

#include <vector>

#include "starboard/common/log.h"

namespace nb {

// Like an std::vector, but supports an additional rewind operation. rewind()
// is like a light-weight clear() operation which does not destroy contained
// objects.
// This is important for performance where the same elements need to be
// re-used. For example, a vector of structs which contain std::strings in
// which it's more desirable for the strings are re-assigned rather than
// destroyed and then re-constructed through rewind() / grow().
//
// Object destruction occurs on clear() or container destruction.
//
// Example:
//  RewindableVector<std::string> values;
//  values.push_back("ABCDEF");
//  values.rewindAll();  // string at [0] is unchanged.
//  EXPECT_EQ(0, values.size());
//  values.expand(1);    // string at [0] is unchanged.
//  values[0].assign("ABC");  // string is re-assigned, no memory allocated.
template <typename T, typename VectorT = std::vector<T> >
class RewindableVector {
 public:
  RewindableVector() : size_(0) {}

  const T& operator[](size_t i) const { return at(i); }
  T& operator[](size_t i) { return at(i); }
  const T& at(size_t i) const {
    SB_DCHECK(i < size_);
    return vector_[i];
  }
  T& at(size_t i) {
    SB_DCHECK(i < size_);
    return vector_[i];
  }

  bool empty() const { return size() == 0; }
  size_t size() const { return size_; }

  void clear() {
    vector_.clear();
    size_ = 0;
  }
  void rewindAll() { size_ = 0; }
  void rewind(size_t i) {
    if (i <= size_) {
      size_ -= i;
    } else {
      SB_NOTREACHED() << "underflow condition.";
      rewindAll();
    }
  }

  // Grows the array by n values. The new last element is returned.
  T& grow(size_t n) {
    size_ += n;
    if (size_ > vector_.size()) {
      vector_.resize(size_);
    }
    return back();
  }
  T& back() { return vector_[size_ - 1]; }

  void pop_back() { rewind(1); }

  void push_back(const T& v) {
    if (size_ == vector_.size()) {
      vector_.push_back(v);
    } else {
      vector_[size_] = v;
    }
    ++size_;
  }

  VectorT& InternalData() { return vector_; }

 private:
  VectorT vector_;
  size_t size_;
};

}  // namespace nb

#endif  // NB_REWINDABLE_VECTOR_CAST_H_
