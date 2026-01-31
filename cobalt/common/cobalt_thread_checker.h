// Copyright 2026 The Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
