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

#ifndef COBALT_H5VCC_H5VCC_SSO_H_
#define COBALT_H5VCC_H5VCC_SSO_H_

#include <memory>
#include <string>

#include "cobalt/script/wrappable.h"
#include "cobalt/sso/sso_interface.h"

namespace cobalt {
namespace h5vcc {

class H5vccSso : public script::Wrappable {
 public:
  H5vccSso();

  std::string GetApiKey();
  std::string GetOauthClientId();
  std::string GetOauthClientSecret();

  DEFINE_WRAPPABLE_TYPE(H5vccSso);

 private:
  std::unique_ptr<sso::SsoInterface> sso_;

  DISALLOW_COPY_AND_ASSIGN(H5vccSso);
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_SSO_H_
