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

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/base_paths_android.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/message_loop/message_pump.h"
#include "base/path_service.h"
#include "cobalt/testing/browser_tests/content_browser_test_shell_main_delegate.h"
#include "content/public/app/content_jni_onload.h"
#include "content/public/app/content_main.h"
#include "content/public/test/nested_message_pump_android.h"
#include "testing/android/native_test/native_test_launcher.h"

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  if (!content::android::OnJNIOnLoadInit()) {
    return -1;
  }

  // Skia needs cobalt_android_fonts.xml to exist on Android, but tests
  // don't run CobaltActivity to copy it. Copy the OS config.
  base::FilePath app_data_dir;
  if (base::PathService::Get(base::DIR_ANDROID_APP_DATA, &app_data_dir)) {
    base::FilePath storage_dir = app_data_dir.Append("storage");
    base::CreateDirectory(storage_dir);
    base::FilePath xml_path = storage_dir.Append("cobalt_android_fonts.xml");
    if (!base::PathExists(xml_path)) {
      base::CopyFile(base::FilePath("/system/etc/fonts.xml"), xml_path);
    }
  }

  // This needs to be done before base::TestSuite::Initialize() is called,
  // as it also tries to set MessagePumpForUIFactory.
  base::MessagePump::OverrideMessagePumpForUIFactory(
      []() -> std::unique_ptr<base::MessagePump> {
        return std::make_unique<content::NestedMessagePumpAndroid>();
      });

  content::SetContentMainDelegate(
      new content::ContentBrowserTestShellMainDelegate());
  return JNI_VERSION_1_4;
}
