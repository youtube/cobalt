// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_TESTING_GTEST_WORKAROUNDS_H_
#define COBALT_DOM_TESTING_GTEST_WORKAROUNDS_H_

#include "base/memory/ref_counted.h"

namespace cobalt {
namespace dom {

// The following set of operators is needed to support EXPECT_* expressions
// where a scoped_refptr object is compared to NULL.
//
// This is needed because the gtest infrastructure is converting the type of
// NULL into "const int".
//
// For example:
//   scoped_refptr<Node> node = ...;
//   EXPECT_EQ(NULL, node);
//
template <typename T>
bool operator==(const int null_value, const scoped_refptr<T>& p) {
  DCHECK_EQ(null_value, 0);
  return p == NULL;
}

template <typename T>
bool operator==(const int null_value, const scoped_refptr<const T>& p) {
  DCHECK_EQ(null_value, 0);
  return p == NULL;
}

template <typename T>
bool operator!=(const int null_value, const scoped_refptr<T>& p) {
  DCHECK_EQ(null_value, 0);
  return p != NULL;
}

template <typename T>
bool operator!=(const int null_value, const scoped_refptr<const T>& p) {
  DCHECK_EQ(null_value, 0);
  return p != NULL;
}

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_TESTING_GTEST_WORKAROUNDS_H_
