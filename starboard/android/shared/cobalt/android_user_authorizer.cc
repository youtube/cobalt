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

#include "starboard/android/shared/cobalt/android_user_authorizer.h"

#include "base/time.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/log.h"

// We're in starboard source, but compiled with Cobalt.
#define STARBOARD_IMPLEMENTATION
#include "starboard/shared/nouser/user_internal.h"
#undef STARBOARD_IMPLEMENTATION

namespace starboard {
namespace android {
namespace shared {
namespace cobalt {

AndroidUserAuthorizer::AndroidUserAuthorizer() {
  JniEnvExt* env = JniEnvExt::Get();
  jobject local_ref = env->CallActivityObjectMethodOrAbort(
      "getUserAuthorizer", "()Lfoo/cobalt/UserAuthorizer;");
  j_user_authorizer_ = env->ConvertLocalRefToGlobalRef(local_ref);
}

AndroidUserAuthorizer::~AndroidUserAuthorizer() {
  JniEnvExt* env = JniEnvExt::Get();
  env->DeleteGlobalRef(j_user_authorizer_);
}

scoped_ptr<AccessToken> AndroidUserAuthorizer::AuthorizeUser(SbUser user) {
  SB_DCHECK(user == &::starboard::shared::nouser::g_user);
  JniEnvExt* env = JniEnvExt::Get();
  jobject j_token = env->CallObjectMethod(
      j_user_authorizer_, "authorizeUser",
      "()Lfoo/cobalt/UserAuthorizer$AccessToken;");
  return CreateAccessToken(j_token);
}

bool AndroidUserAuthorizer::DeauthorizeUser(SbUser user) {
  SB_DCHECK(user == &::starboard::shared::nouser::g_user);
  JniEnvExt* env = JniEnvExt::Get();
  return env->CallBooleanMethod(
      j_user_authorizer_, "deauthorizeUser", "()z");
}

scoped_ptr<AccessToken>
AndroidUserAuthorizer::RefreshAuthorization(SbUser user) {
  SB_DCHECK(user == &::starboard::shared::nouser::g_user);
  JniEnvExt* env = JniEnvExt::Get();
  jobject j_token = env->CallObjectMethod(
      j_user_authorizer_, "refreshAuthorization",
      "()Lfoo/cobalt/UserAuthorizer$AccessToken;");
  return CreateAccessToken(j_token);
}

scoped_ptr<AccessToken>
AndroidUserAuthorizer::CreateAccessToken(jobject j_token) {
  if (!j_token) {
    return scoped_ptr<AccessToken>(NULL);
  }

  JniEnvExt* env = JniEnvExt::Get();
  scoped_ptr<AccessToken> access_token(new AccessToken());

  jstring j_token_string = (jstring)env->CallObjectMethodOrAbort(
      j_token, "getTokenValue", "()Ljava/lang/String;");
  if (j_token_string) {
    const char* utf_chars = env->GetStringUTFChars(j_token_string, NULL);
    access_token->token_value.assign(utf_chars);
    env->ReleaseStringUTFChars(j_token_string, utf_chars);
  }

  jlong j_expiry = env->CallLongMethodOrAbort(
      j_token, "getExpirySeconds", "()J");
  if (j_expiry > 0) {
    access_token->expiry =
        base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(j_expiry);
  }

  return access_token.Pass();
}

}  // namespace cobalt
}  // namespace shared
}  // namespace android
}  // namespace starboard

namespace cobalt {
namespace account {

UserAuthorizer* UserAuthorizer::Create() {
  return new ::starboard::android::shared::cobalt::AndroidUserAuthorizer();
}

}  // namespace account
}  // namespace cobalt
