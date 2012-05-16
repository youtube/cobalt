// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/base_jni_registrar.h"

#include "base/basictypes.h"
#include "base/android/build_info.h"
#include "base/android/locale_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/android/path_utils.h"

namespace base {
bool RegisterSystemMessageHandler(JNIEnv* env);
}

namespace base {
namespace android {

static RegistrationMethod kBaseRegisteredMethods[] = {
  { "BuildInfo", base::android::RegisterBuildInfo },
  { "LocaleUtils", base::android::RegisterLocaleUtils },
  { "PathUtils", base::android::RegisterPathUtils },
  { "SystemMessageHandler", base::RegisterSystemMessageHandler },
};

bool RegisterJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kBaseRegisteredMethods,
                               arraysize(kBaseRegisteredMethods));
}

}  // namespace android
}  // namespace base
