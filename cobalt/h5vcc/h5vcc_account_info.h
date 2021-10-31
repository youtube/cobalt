// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_H5VCC_H5VCC_ACCOUNT_INFO_H_
#define COBALT_H5VCC_H5VCC_ACCOUNT_INFO_H_

#include <string>

#include "cobalt/account/account_manager.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace h5vcc {

class H5vccAccountInfo : public script::Wrappable {
 public:
  explicit H5vccAccountInfo(account::AccountManager* account_manager);
  std::string avatar_url() const;
  std::string username() const;
  std::string user_id() const;

  DEFINE_WRAPPABLE_TYPE(H5vccAccountInfo);

 private:
  account::AccountManager* account_manager_;
  DISALLOW_COPY_AND_ASSIGN(H5vccAccountInfo);
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_ACCOUNT_INFO_H_
