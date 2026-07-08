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

#ifndef COBALT_BROWSER_CLIENT_HINT_HEADERS_COBALT_HEADER_VALUE_PROVIDER_H_
#define COBALT_BROWSER_CLIENT_HINT_HEADERS_COBALT_HEADER_VALUE_PROVIDER_H_

#include <map>
#include <string>

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"

namespace cobalt {
namespace browser {

// Client hint header name constants.
constexpr char kCobaltCertScopeHeaderName[] =
    "Sec-CH-UA-Co-Youtube-Certification-Scope";
constexpr char kAndroidOSExperienceHeaderName[] =
    "Sec-CH-UA-Co-Android-OS-Experience";
constexpr char kPlayServicesVersionHeaderName[] =
    "Sec-CH-UA-Co-Android-Play-Services-Version";
constexpr char kBuildFingerprintHeaderName[] =
    "Sec-CH-UA-Co-Android-Build-Fingerprint";

// Thread-safe provider for Cobalt-specific trusted client hint HTTP headers.
// Holds global header key-value pairs attached to outgoing network requests.
class CobaltHeaderValueProvider {
 public:
  using HeaderMap = std::map<std::string, std::string>;

  // Returns the process-wide singleton instance of CobaltHeaderValueProvider.
  static CobaltHeaderValueProvider* GetInstance();

  CobaltHeaderValueProvider(const CobaltHeaderValueProvider&) = delete;
  CobaltHeaderValueProvider& operator=(const CobaltHeaderValueProvider&) =
      delete;

  // Thread-safely sets or updates a client hint header key-value pair.
  void SetHeaderValue(const std::string& header_name,
                      const std::string& header_value);

  // Thread-safely returns a copy of all current client hint header key-value
  // pairs.
  HeaderMap GetHeaderValues() const;

  // Clears all stored header values for test isolation.
  void ClearHeaderValuesForTesting();

  // Re-initializes system property client hint headers for testing.
  void LoadSystemProperties();

 private:
  friend class base::NoDestructor<CobaltHeaderValueProvider>;

  // Private constructor and destructor to enforce singleton usage via
  // GetInstance() and base::NoDestructor.
  CobaltHeaderValueProvider();
  ~CobaltHeaderValueProvider() = default;

  // Guard for concurrent access to header_values_.
  mutable base::Lock lock_;

  // Map of header name to header value.
  HeaderMap header_values_ GUARDED_BY(lock_);
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_CLIENT_HINT_HEADERS_COBALT_HEADER_VALUE_PROVIDER_H_
