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

#ifndef STARBOARD_ANDROID_SHARED_COBALT_ANDROID_USER_AUTHORIZER_H_
#define STARBOARD_ANDROID_SHARED_COBALT_ANDROID_USER_AUTHORIZER_H_

#include <memory>

#include "cobalt/account/user_authorizer.h"

#include "starboard/android/shared/jni_env_ext.h"

namespace starboard {
namespace android {
namespace shared {
namespace cobalt {

using ::cobalt::account::AccessToken;

// Android implementation of UserAuthorizer.
//
// Signing-in is a higher-level concept that is implemented by the Android app
// using the Android AccountManager and/or Google Play Services to select an
// account and get auth tokens for the selected account to make "signed-in"
// requests.  Account selection is accomplished by prompting the user as needed
// when getting a token with AuthorizeUser(), and remembering the selected
// account to be used without prompting in RefreshAuthorization().
class AndroidUserAuthorizer : public ::cobalt::account::UserAuthorizer {
 public:
  AndroidUserAuthorizer();
  ~AndroidUserAuthorizer() override;

  std::unique_ptr<AccessToken> AuthorizeUser() override;
  bool DeauthorizeUser() override;
  std::unique_ptr<AccessToken> RefreshAuthorization() override;
  void Shutdown() override;

 private:
  std::unique_ptr<AccessToken> CreateAccessToken(jobject j_token);

  jobject j_user_authorizer_;

  bool shutdown_;
};

}  // namespace cobalt
}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_COBALT_ANDROID_USER_AUTHORIZER_H_
