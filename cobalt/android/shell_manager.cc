// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/android/shell_manager.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "cobalt/cobalt_shell.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/android/content_shell_jni_headers/ShellManager_jni.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "url/gurl.h"

// Note: Origin of this file is content/shell/shell_manager.cc from m114

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace {

struct GlobalState {
  GlobalState() {}
  base::android::ScopedJavaGlobalRef<jobject> j_shell_manager;
};

base::LazyInstance<GlobalState>::DestructorAtExit g_global_state =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace cobalt {

ScopedJavaLocalRef<jobject> CreateShellView(content::Shell* shell) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return content::Java_ShellManager_createShell(
      env, g_global_state.Get().j_shell_manager,
      reinterpret_cast<intptr_t>(shell));
}

void RemoveShellView(const JavaRef<jobject>& shell_view) {
  JNIEnv* env = base::android::AttachCurrentThread();
  content::Java_ShellManager_removeShell(
      env, g_global_state.Get().j_shell_manager, shell_view);
}

}  // namespace cobalt

// Note: Below tracks generated Java code, which is currently generated
// in content, not cobalt namespace.

namespace content {

static void JNI_ShellManager_Init(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj) {
  g_global_state.Get().j_shell_manager.Reset(obj);
}

void JNI_ShellManager_LaunchShell(JNIEnv* env,
                                  const JavaParamRef<jstring>& jurl) {
  content::ShellBrowserContext* browserContext =
      content::ShellContentBrowserClient::Get()->browser_context();
  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));

  // Cobalt specific shell creation
  cobalt::CobaltShell::CreateNewWindow(browserContext, url, nullptr,
                                       gfx::Size());
}

void DestroyShellManager() {
  JNIEnv* env = base::android::AttachCurrentThread();
  content::Java_ShellManager_destroy(env, g_global_state.Get().j_shell_manager);
}

}  // namespace content
