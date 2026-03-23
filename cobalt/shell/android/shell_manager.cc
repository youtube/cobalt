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

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "cobalt/shell/android/cobalt_shell_jni_headers/ShellManager_jni.h"
#include "cobalt/shell/browser/migrate_storage_record/migration_manager.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_browser_context.h"
#include "cobalt/shell/browser/shell_browser_main_parts.h"
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
                                  const JavaParamRef<jstring>& jurl,
                                  const JavaParamRef<jstring>& jdeeplink_url) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));
  std::string deeplink_url =
      base::android::ConvertJavaStringToUTF8(env, jdeeplink_url);

  // We wrap the WebContents creation into a task because we must ensure
  // that any asynchronous storage migration is completely finished before
  // the browser accesses the default storage partition. If WebContents
  // is created too early, the JS environment will start with empty data.
  auto create_window_task = base::BindOnce(
      [](GURL url, std::string deeplink_url) {
        const std::string status_param = cobalt::migrate_storage_record::
            MigrationManager::GetMigrationStatusUrlParameter();
        if (!status_param.empty() && !deeplink_url.empty()) {
          // If a migration occurred on this launch, append its outcome directly
          // to the URL's query parameters (e.g. "?migration_status=0-0-0").
          // This makes the migration telemetry accessible to the loaded web
          // app.
          if (deeplink_url.find('?') == std::string::npos) {
            deeplink_url += "?";
          } else {
            deeplink_url += "&";
          }
          deeplink_url += status_param;
        }
        ShellBrowserContext* browserContext =
            ShellContentBrowserClient::Get()->browser_context();
        Shell::CreateNewWindow(browserContext, url, nullptr, gfx::Size(),
                               switches::ShouldCreateSplashScreen(),
                               deeplink_url);
      },
      std::move(url), std::move(deeplink_url));

  auto* parts = ShellContentBrowserClient::Get()->shell_browser_main_parts();
  if (parts) {
    // Defers the execution of the task if the migration is still in progress.
    // If the migration is already complete, the task is executed synchronously.
    parts->PostOrRunIfStorageMigrationFinished(std::move(create_window_task));
  } else {
    std::move(create_window_task).Run();
  }
}

void DestroyShellManager() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ShellManager_destroy(env, g_global_state.Get().j_shell_manager);
}

}  // namespace content
