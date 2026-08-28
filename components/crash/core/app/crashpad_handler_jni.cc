// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "third_party/crashpad/crashpad/client/client_argv_handling.h"
#include "third_party/crashpad/crashpad/handler/handler_main.h"

#include <android/log.h>
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, "CrashpadMainNative", __VA_ARGS__)

#include "base/android/cobalt_for_google3_buildflags.h"

#if BUILDFLAG(IS_COBALT_ON_GOOGLE3)
#define JNI_METHOD_NAME(name) Java_cobalt_org_chromium_components_crash_browser_CrashpadMainCobalt_##name
#elif BUILDFLAG(IS_COBALT) && BUILDFLAG(IS_ANDROIDTV)
#define JNI_METHOD_NAME(name) Java_org_chromium_components_crash_browser_CrashpadMainCobalt_##name
#else
#define JNI_METHOD_NAME(name) Java_org_chromium_components_crash_browser_CrashpadMain_##name
#endif

namespace crashpad {

extern "C" {

/**
 * Native implementation of CrashpadMain.nativeCrashpadMain for Cobalt.
 * This uses a traditional JNI name to avoid GEN_JNI collisions in monolithic builds.
 */
JNIEXPORT void JNICALL
JNI_METHOD_NAME(nativeCrashpadMain)(
    JNIEnv* env,
    jclass jcaller,
    jobjectArray j_argv) {
  ALOGI("Native: CrashpadMain.nativeCrashpadMain started");
  std::vector<std::string> argv_strings;
  base::android::AppendJavaStringArrayToStringVector(
      env, base::android::JavaParamRef<jobjectArray>(env, j_argv),
      &argv_strings);

  std::vector<const char*> argv;
  StringVectorToCStringVector(argv_strings, &argv);
  
  ALOGI("Native: Transitioning to CrashpadHandlerMain with %zu args", argv.size() - 1);
  CrashpadHandlerMain(argv.size() - 1, const_cast<char**>(argv.data()));
}

}  // extern "C"

}  // namespace crashpad
