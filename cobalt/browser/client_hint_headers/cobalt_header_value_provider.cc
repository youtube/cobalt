// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/client_hint_headers/cobalt_header_value_provider.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "starboard/common/system_property.h"

namespace cobalt {
namespace browser {

// static
CobaltHeaderValueProvider* CobaltHeaderValueProvider::GetInstance() {
  static base::NoDestructor<CobaltHeaderValueProvider> instance;
  return instance.get();
}

CobaltHeaderValueProvider::CobaltHeaderValueProvider() {
  LoadSystemPropertiesForTesting();
}

CobaltHeaderValueProvider::HeaderMap
CobaltHeaderValueProvider::GetHeaderValues() const {
  // Thread-safely acquire lock and return a copy of current header values.
  base::AutoLock auto_lock(lock_);
  return header_values_;
}

void CobaltHeaderValueProvider::SetHeaderValue(
    const std::string& header_name,
    const std::string& header_value) {
  // Thread-safely acquire lock and insert or update the specified header key.
  base::AutoLock auto_lock(lock_);
  header_values_.insert_or_assign(header_name, header_value);
}

void CobaltHeaderValueProvider::ClearHeaderValuesForTesting() {
  // Thread-safely acquire lock and clear stored headers for test isolation.
  base::AutoLock auto_lock(lock_);
  header_values_.clear();
}

void CobaltHeaderValueProvider::LoadSystemPropertiesForTesting() {
#if BUILDFLAG(IS_STARBOARD)
  // Retrieve the platform certification scope upon singleton initialization.
  // If present on the underlying platform/device, store it so it is
  // automatically attached as a trusted client hint header.
  std::string cert_scope =
      starboard::GetSystemPropertyString(kSbSystemPropertyCertificationScope);
  if (!cert_scope.empty()) {
    SetHeaderValue(kCobaltCertScopeHeaderName, cert_scope);
  }
#endif
}

}  // namespace browser
}  // namespace cobalt
