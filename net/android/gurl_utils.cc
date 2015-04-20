// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/gurl_utils.h"

#include "base/android/jni_string.h"
#include "googleurl/src/gurl.h"
#include "jni/GURLUtils_jni.h"

namespace net {

jstring GetOrigin(JNIEnv* env, jclass clazz, jstring url) {
  GURL host(base::android::ConvertJavaStringToUTF16(env, url));

  return base::android::ConvertUTF8ToJavaString(env,
      host.GetOrigin().spec()).Release();
}

bool RegisterGURLUtils(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // net namespace
