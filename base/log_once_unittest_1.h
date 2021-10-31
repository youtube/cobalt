// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#ifndef BASE_LOG_ONCE_UNITTEST_1_H_
#define BASE_LOG_ONCE_UNITTEST_1_H_

#include "base/logging.h"

// Empty lines for alignment follow.
// ...
// ...
// ...
static inline void log_once_unittest_1() {
  // Note: The LOG_ONCE statement below must remain on the same line as the
  // LOG_ONCE statements in log_once_unittest_2.h and logging_unittest.c
  LOG_ONCE(INFO) << "This is log_once_unittest_1().";
  SB_DCHECK(__LINE__ == 28);  // Note keep in sync with the other statements.
}

static inline void notimplemented_log_once_unittest_1() {
  // Note: The NOTIMPLEMENTED_LOG_ONCE statement below must remain on the same
  // line as the NOTIMPLEMENTED_LOG_ONCE statements in log_once_unittest_2.h
  // and logging_unittest.c
  NOTIMPLEMENTED_LOG_ONCE() << "This is notimplemented_log_once_unittest_1().";
  SB_DCHECK(__LINE__ == 36);  // Note keep in sync with the other statements.
}

#endif  // BASE_LOG_ONCE_UNITTEST_1_H_
