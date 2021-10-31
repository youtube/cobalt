// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MATH_RATIONAL_H_
#define COBALT_MATH_RATIONAL_H_

namespace cobalt {
namespace math {

// Describes a rational number and maintains two separate integer values for the
// numerator and denominator.
class Rational {
 public:
  Rational(int numerator, int denominator)
      : numerator_(numerator), denominator_(denominator) {}

  int numerator() const { return numerator_; }
  int denominator() const { return denominator_; }

  bool operator==(const Rational& other) const {
    return static_cast<int64>(numerator_) * other.denominator_ ==
           static_cast<int64>(denominator_) * other.numerator_;
  }

  bool operator>(const Rational& other) const {
    return static_cast<int64>(numerator_) * other.denominator_ >
           static_cast<int64>(denominator_) * other.numerator_;
  }

  bool operator>=(const Rational& other) const {
    return static_cast<int64>(numerator_) * other.denominator_ >=
           static_cast<int64>(denominator_) * other.numerator_;
  }

  bool operator<(const Rational& other) const {
    return static_cast<int64>(numerator_) * other.denominator_ <
           static_cast<int64>(denominator_) * other.numerator_;
  }

  bool operator<=(const Rational& other) const {
    return static_cast<int64>(numerator_) * other.denominator_ <=
           static_cast<int64>(denominator_) * other.numerator_;
  }

 private:
  int numerator_;
  int denominator_;
};

}  // namespace math
}  // namespace cobalt

#endif  // COBALT_MATH_RATIONAL_H_
