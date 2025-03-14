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

#ifndef COBALT_IFA_IFA_H_
#define COBALT_IFA_IFA_H_

#include "base/memory/raw_ptr.h"
#include "starboard/extension/ifa.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace cobalt {
namespace ifa {

#error TODO write class comment
class Ifa {
 public:
  static Ifa* GetInstance();

  std::string tracking_authorization_status() const;
  bool RequestTrackingAuthorization(/*pass promise here?*/);

 private:
  Ifa();
  Ifa(const Ifa&) = delete;
  Ifa& operator=(const Ifa&) = delete;

  void ReceiveTrackingAuthorizationComplete();

  friend struct base::DefaultSingletonTraits<Ifa>;
  raw_ptr<const StarboardExtensionIfaApi> configuration_api_;

  std::vector<std::unique_ptr<script::ValuePromiseVoid::Reference>>
      request_tracking_authorization_promises_;  // track void promises with
                                                 // this
};

}  // namespace ifa
}  // namespace cobalt

#endif  // COBALT_IFA_IFA_H_
