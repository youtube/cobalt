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

#include "starboard/android/shared/cobalt/android_user_authorizer.h"

#include <memory>

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/common/log.h"

namespace starboard {
namespace android {
namespace shared {
namespace cobalt {

bool UserAuthorizerIsSupported() {
  // If using the NoopUserAuthorizer, then user authorizer functionally is not
  // supported.
  JniEnvExt* env = JniEnvExt::Get();
  jobject local_ref = env->CallStarboardObjectMethodOrAbort(
      "getUserAuthorizer", "()Ldev/cobalt/account/UserAuthorizer;");
  return !env->IsInstanceOf(
      local_ref,
      env->FindClassExtOrAbort("dev/cobalt/account/NoopUserAuthorizer"));
}

AndroidUserAuthorizer::AndroidUserAuthorizer() : shutdown_(false) {
  JniEnvExt* env = JniEnvExt::Get();
  jobject local_ref = env->CallStarboardObjectMethodOrAbort(
      "getUserAuthorizer", "()Ldev/cobalt/account/UserAuthorizer;");
  j_user_authorizer_ = env->ConvertLocalRefToGlobalRef(local_ref);
}

AndroidUserAuthorizer::~AndroidUserAuthorizer() {
  JniEnvExt* env = JniEnvExt::Get();
  env->DeleteGlobalRef(j_user_authorizer_);
}

void AndroidUserAuthorizer::Shutdown() {
  shutdown_ = true;
  JniEnvExt* env = JniEnvExt::Get();
  env->CallVoidMethodOrAbort(j_user_authorizer_, "interrupt", "()V");
}

std::unique_ptr<AccessToken> AndroidUserAuthorizer::AuthorizeUser() {
  if (shutdown_) {
    DLOG(WARNING) << "No-op AuthorizeUser after shutdown";
    return std::unique_ptr<AccessToken>();
  }
  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jobject> j_token(
      env->CallObjectMethodOrAbort(j_user_authorizer_, "authorizeUser",
                                   "()Ldev/cobalt/account/AccessToken;"));
  return CreateAccessToken(j_token.Get());
}

bool AndroidUserAuthorizer::DeauthorizeUser() {
  if (shutdown_) {
    DLOG(WARNING) << "No-op DeauthorizeUser after shutdown";
    return false;
  }
  JniEnvExt* env = JniEnvExt::Get();
  return env->CallBooleanMethodOrAbort(j_user_authorizer_, "deauthorizeUser",
                                       "()Z");
}

std::unique_ptr<AccessToken> AndroidUserAuthorizer::RefreshAuthorization() {
  if (shutdown_) {
    DLOG(WARNING) << "No-op RefreshAuthorization after shutdown";
    return std::unique_ptr<AccessToken>();
  }
  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jobject> j_token(
      env->CallObjectMethodOrAbort(j_user_authorizer_, "refreshAuthorization",
                                   "()Ldev/cobalt/account/AccessToken;"));
  return CreateAccessToken(j_token.Get());
}

std::unique_ptr<AccessToken> AndroidUserAuthorizer::CreateAccessToken(
    jobject j_token) {
  if (!j_token) {
    return std::unique_ptr<AccessToken>();
  }

  JniEnvExt* env = JniEnvExt::Get();
  std::unique_ptr<AccessToken> access_token(new AccessToken());

  ScopedLocalJavaRef<jstring> j_token_string(env->CallObjectMethodOrAbort(
      j_token, "getTokenValue", "()Ljava/lang/String;"));
  if (j_token_string) {
    access_token->token_value =
        env->GetStringStandardUTFOrAbort(j_token_string.Get());
  }

  jlong j_expiry =
      env->CallLongMethodOrAbort(j_token, "getExpirySeconds", "()J");
  if (j_expiry > 0) {
    access_token->expiry =
        base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(j_expiry);
  }

  return access_token;
}

}  // namespace cobalt
}  // namespace shared
}  // namespace android
}  // namespace starboard

namespace cobalt {
namespace account {

UserAuthorizer* UserAuthorizer::Create() {
  if (::starboard::android::shared::cobalt::UserAuthorizerIsSupported()) {
    return new ::starboard::android::shared::cobalt::AndroidUserAuthorizer();
  }
  return nullptr;
}

}  // namespace account
}  // namespace cobalt
