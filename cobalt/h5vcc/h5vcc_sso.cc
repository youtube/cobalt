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

#include "cobalt/h5vcc/h5vcc_sso.h"

namespace cobalt {
namespace h5vcc {

H5vccSso::H5vccSso() : sso_(std::move(sso::CreateSSO())) {}

std::string H5vccSso::GetApiKey() {
  DCHECK(sso_);
  return sso_->getApiKey();
}

std::string H5vccSso::GetOauthClientId() {
  DCHECK(sso_);
  return sso_->getOauthClientId();
}

std::string H5vccSso::GetOauthClientSecret() {
  DCHECK(sso_);
  return sso_->getOauthClientSecret();
}

}  // namespace h5vcc
}  // namespace cobalt
