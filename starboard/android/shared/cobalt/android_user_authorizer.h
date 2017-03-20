// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_COBALT_ANDROID_USER_AUTHORIZER_H_
#define STARBOARD_ANDROID_SHARED_COBALT_ANDROID_USER_AUTHORIZER_H_

#include "cobalt/account/user_authorizer.h"

namespace starboard {
namespace android {
namespace shared {
namespace cobalt {

using ::cobalt::account::AccessToken;

class AndroidUserAuthorizer : public ::cobalt::account::UserAuthorizer {
 public:
  scoped_ptr<AccessToken> AuthorizeUser(SbUser user) SB_OVERRIDE;
  bool DeauthorizeUser(SbUser user) SB_OVERRIDE;
  scoped_ptr<AccessToken> RefreshAuthorization(SbUser user) SB_OVERRIDE;
};

}  // namespace cobalt
}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_COBALT_ANDROID_USER_AUTHORIZER_H_
