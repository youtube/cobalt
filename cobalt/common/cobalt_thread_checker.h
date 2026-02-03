// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_COMMON_COBALT_THREAD_CHECKER_H_
#define COBALT_COMMON_COBALT_THREAD_CHECKER_H_

#include "base/logging.h"
#include "base/macros/uniquify.h"
#include "base/threading/thread_checker_impl.h"

// Same macros as in base/threading/thread_checker.h (e.g. THREAD_CHECKER(name),
// DCHECK_CALLED_ON_VALID_THREAD, etc) but for our always-enabled purposes.
#define COBALT_THREAD_CHECKER(name) cobalt::common::CobaltThreadChecker name
#define CHECK_CALLED_ON_VALID_THREAD(name, ...) \
  cobalt::common::ScopedValidateCobaltThreadChecker \
      BASE_UNIQUIFY(scoped_validate_thread_checker_)(name, ##__VA_ARGS__);
#define COBALT_DETACH_FROM_THREAD(name) (name).DetachFromThread()

namespace cobalt {
namespace common {

// Same as base::ThreadChecker but always enabled, not just when DCHECK_IS_ON().
// Thread checking is cheap but not free - this  class is intended to be used on
// creation/construction and otherwise low-traffic methods, to not introduce
// performance regressions.
class CobaltThreadChecker : public base::ThreadCheckerImpl {};

class SCOPED_LOCKABLE ScopedValidateCobaltThreadChecker {
 public:
  explicit ScopedValidateCobaltThreadChecker(
      const CobaltThreadChecker& checker) {
    CHECK(checker.CalledOnValidThread());
  }

  ScopedValidateCobaltThreadChecker(const ScopedValidateCobaltThreadChecker&) =
      delete;
  ScopedValidateCobaltThreadChecker& operator=(
      const ScopedValidateCobaltThreadChecker&) = delete;

  ~ScopedValidateCobaltThreadChecker() {}
};

}  // namespace common
}  // namespace cobalt

#endif  // COBALT_COMMON_COBALT_THREAD_CHECKER_H_
