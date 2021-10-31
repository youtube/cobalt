/*
 * Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MATH_LINEAR_INTERPOLATOR_H_
#define COBALT_MATH_LINEAR_INTERPOLATOR_H_

#include <iterator>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "starboard/types.h"

namespace cobalt {
namespace math {

// LinearInterpolator effectively generates outputs based on a table. This can
// be used for function approximation (ie. sine) or arbitrary functions that
// need specific values at specific inputs, and interpolated values otherwise.
// KeyT and ValueT can be any builtin integer and float point type.
// It's recommended that double types are used unless speed is required, then
// use other types.
//
// Performance:
//  This table increases the computation on the order of O(n) of the number
//  of interpolation points. However in practice the number of interpolation
//  points is small enough (<20) where this linear search is faster than binary
//  lookup.
//
// Discontinuous values:
//  This table supports discontinuous values. To enable this feature, insert a
//  pair of duplicate keys, each with a different value. See example below.
//
// Example (continuous):
//  LinearInterpolator<float, float> interp;
//  interp.Add(0, 0);
//  interp.Add(1, 2);
//  EXPECT_FLOAT_EQ(0.f, interp.Map(0.0f));
//  EXPECT_FLOAT_EQ(1.f, interp.Map(0.5f));
//  EXPECT_FLOAT_EQ(2.f, interp.Map(1.0f));
//
// Example (discontinuous):
//  LinearInterpolator<double, float> interp;
//  interp.Add(0, 0);
//  interp.Add(1, 1);  // Discontinuity at input = 1.
//  interp.Add(1, 3);
//  interp.Add(2, 4);
//  static const double kErrorThreshold = .1f;
//  static const double kEpsilon = std::numeric_limits<double>::epsilon()*4.0;
//  EXPECT_NEAR(1.0, interp.Map(1.f - kEpsilon), kErrorThreshold);
//  EXPECT_FLOAT_EQ(3, interp.Map(1.f));
//  EXPECT_NEAR(3.0, interp.Map(1.f + kEpsilon), kErrorThreshold);
template <typename KeyT, typename ValueT>
class LinearInterpolator {
 public:
  ValueT Map(const KeyT& key) const { return Interp(key); }

  // Adds a key to the table. For a continuous piecewise line-curve each key
  // added must be greater in value than the last key added.
  // If duplicate keys are entered sequentially then this will introduce
  // a discontinuity - a big jump in the returned value from Map() when the
  // input crosses the discontinuity point.
  void Add(const KeyT& key, const ValueT& val) {
    if (!table_.empty()) {
      DCHECK_LE(table_.back().first, key) << "Keys are not in order.";
    }
    table_.push_back(std::pair<KeyT, ValueT>(key, val));
  }
  void Clear() { table_.clear(); }

 private:
  ValueT Interp(const KeyT& k) const {
    if (table_.empty()) {
      return ValueT(0);
    }

    // Contains the indices which reference keys before and after the key.
    std::pair<size_t, size_t> indices = SelectInterpPoints(k);

    // Singular value, no interpolation needed.
    if (indices.first == indices.second) {
      return table_[indices.first].second;
    }

    // Perform the interpolation with the given key & value lines.
    const std::pair<KeyT, ValueT>& curr = table_[indices.first];
    const std::pair<KeyT, ValueT>& next = table_[indices.second];

    // Duplicate keys, select the second value. This prevents
    // LinearInterpolation() from dividing by 0 for duplicate keys.
    if (curr.first == next.first) {
      return next.second;
    }

    return LinearInterpolation(k, curr.first, next.first, curr.second,
                               next.second);
  }

  // Linearly walk through the table looking for a pair of interpolation
  // points. Linear walking must be used because this table uses duplicate
  // keys generate discontinuous outputs.
  std::pair<size_t, size_t> SelectInterpPoints(const KeyT& k) const {
    DCHECK(!table_.empty());
    if (k < table_[0].first) {
      return std::pair<size_t, size_t>(0, 0);
    }

    for (size_t i = 0; i < table_.size() - 1; ++i) {
      const std::pair<KeyT, ValueT>& curr = table_[i];
      const std::pair<KeyT, ValueT>& next = table_[i + 1];

      if (curr.first <= k && k < next.first) {
        return std::pair<size_t, size_t>(i, i + 1);
      }
    }

    return std::pair<size_t, size_t>(table_.size() - 1, table_.size() - 1);
  }

  // Maps a value in the range [x1, x2] to the range [y1, y2].
  static const ValueT LinearInterpolation(const KeyT& t, const KeyT& x1,
                                          const KeyT& x2, const ValueT& y1,
                                          const ValueT& y2) {
    ValueT return_val =
        static_cast<ValueT>((t - x1) * (y2 - y1) / (x2 - x1) + y1);
    return return_val;
  }

  typedef std::vector<std::pair<KeyT, ValueT> > VectorType;
  VectorType table_;
};

}  // namespace math
}  // namespace cobalt

#endif  // COBALT_MATH_LINEAR_INTERPOLATOR_H_
