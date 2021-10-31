// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_SSO_SSO_INTERFACE_H_
#define COBALT_SSO_SSO_INTERFACE_H_

#include <memory>
#include <string>


namespace cobalt {
namespace sso {

// Porters should inherit from this class, and implement the pure
// virtual functions.
class SsoInterface {
 public:
  virtual ~SsoInterface() {}
  virtual std::string getApiKey() const = 0;
  virtual std::string getOauthClientId() const = 0;
  virtual std::string getOauthClientSecret() const = 0;
};

// Porters should implement this function in the |starboard|
// directory, and link the definition inside cobalt.
std::unique_ptr<SsoInterface> CreateSSO();

}  // namespace sso
}  // namespace cobalt

#endif  // COBALT_SSO_SSO_INTERFACE_H_
