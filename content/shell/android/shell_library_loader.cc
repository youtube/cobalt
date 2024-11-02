// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "content/public/app/content_jni_onload.h"
#include "content/public/app/content_main.h"
#include "content/shell/app/shell_main_delegate.h"

#include <android/log.h>

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  __android_log_print(ANDROID_LOG_ERROR, "yolo", "%s", "JNI_OnLoad from shell_lib_loaded!!");
  base::android::InitVM(vm);
  if (!content::android::OnJNIOnLoadInit())
    return -1;
  content::SetContentMainDelegate(new content::ShellMainDelegate());
  return JNI_VERSION_1_4;
}
