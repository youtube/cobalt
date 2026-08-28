// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(ANDROID)
#include <jni.h>
#include "base/android/jni_android.h"
#endif

// Forward declare this for macOS (it's only defined by crashpad on Android.)
extern "C" int CrashpadHandlerMain(int argc, char* argv[]);

int main(int argc, char* argv[]) {
  return CrashpadHandlerMain(argc, argv);
}

#if defined(ANDROID)
extern "C" {

JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  return JNI_VERSION_1_4;
}

}  // extern "C"
#endif
