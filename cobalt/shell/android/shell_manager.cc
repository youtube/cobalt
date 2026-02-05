// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/shell/android/shell_manager.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "cobalt/shell/android/cobalt_shell_jni_headers/ShellManager_jni.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_browser_context.h"
#include "cobalt/shell/browser/shell_content_browser_client.h"
#include "cobalt/shell/common/shell_switches.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

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

namespace content {

ScopedJavaLocalRef<jobject> CreateShellView(Shell* shell) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ShellManager_createShell(env,
                                       g_global_state.Get().j_shell_manager,
                                       reinterpret_cast<intptr_t>(shell));
}

void RemoveShellView(const JavaRef<jobject>& shell_view) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ShellManager_removeShell(env, g_global_state.Get().j_shell_manager,
                                shell_view);
}

static void JNI_ShellManager_Init(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj) {
  g_global_state.Get().j_shell_manager.Reset(obj);
}

void JNI_ShellManager_LaunchShell(JNIEnv* env,
                                  const JavaParamRef<jstring>& jurl) {
  ShellBrowserContext* browserContext =
      ShellContentBrowserClient::Get()->browser_context();
  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));
  Shell::CreateNewWindow(browserContext, url, nullptr, gfx::Size(),
                         switches::ShouldCreateSplashScreen());
}

void DestroyShellManager() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ShellManager_destroy(env, g_global_state.Get().j_shell_manager);
}

}  // namespace content
