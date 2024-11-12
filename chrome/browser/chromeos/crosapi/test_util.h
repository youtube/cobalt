// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSAPI_TEST_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_CROSAPI_TEST_UTIL_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/token.h"

namespace crosapi {

namespace internal {
int GetInterfaceVersionImpl(base::Token interface_uuid);
}  // namespace internal

// Abstraction over testing crosapi::browser_util::GetAshCapabilities() that
// works in both the Lacros and Ash processes.
bool AshSupportsCapabilities(const base::flat_set<std::string>& capabilities);

// Abstraction over LacrosService::GetInterfaceVersion() that can be called
// in Lacros or Ash.
template <typename T>
int GetInterfaceVersion() {
  return internal::GetInterfaceVersionImpl(T::Uuid_);
}

}  // namespace crosapi

#endif  // CHROME_BROWSER_CHROMEOS_CROSAPI_TEST_UTIL_H_
