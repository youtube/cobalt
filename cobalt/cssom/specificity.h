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

#ifndef COBALT_CSSOM_SPECIFICITY_H_
#define COBALT_CSSOM_SPECIFICITY_H_

#include "base/basictypes.h"
#include "base/logging.h"

namespace cobalt {
namespace cssom {

// A selector's specificity is calculated as follows:
// Count the number of ID selectors in the selector (A)
// Count the number of class selectors, attributes selectors, and
// pseudo-classes in the selector (B)
// Count the number of type selectors and pseudo-elements in the selector (C)
//   https://www.w3.org/TR/selectors4/#specificity
// When adding two specificities, clampping the result at each field (A, B and
// C).
class Specificity {
 public:
  Specificity() {
    v_[0] = 0;
    v_[1] = 0;
    v_[2] = 0;
  }

  Specificity(int8 a, int8 b, int8 c) {
    DCHECK_GE(a, 0) << "Specificity field cannot be negative.";
    DCHECK_GE(b, 0) << "Specificity field cannot be negative.";
    DCHECK_GE(c, 0) << "Specificity field cannot be negative.";
    v_[0] = a;
    v_[1] = b;
    v_[2] = c;
  }

  // Adds the value of another specificity to self.
  void AddFrom(const Specificity& rhs);

  // Specificity fields are compared lexicographically.
  bool operator<(const Specificity& rhs) const {
    return v_[0] < rhs.v_[0] ||
           (v_[0] == rhs.v_[0] &&
            (v_[1] < rhs.v_[1] || (v_[1] == rhs.v_[1] && v_[2] < rhs.v_[2])));
  }
  bool operator>(const Specificity& rhs) const {
    return v_[0] > rhs.v_[0] ||
           (v_[0] == rhs.v_[0] &&
            (v_[1] > rhs.v_[1] || (v_[1] == rhs.v_[1] && v_[2] > rhs.v_[2])));
  }
  bool operator==(const Specificity& rhs) const {
    return v_[0] == rhs.v_[0] && v_[1] == rhs.v_[1] && v_[2] == rhs.v_[2];
  }

 private:
  int8 v_[3];
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_SPECIFICITY_H_
