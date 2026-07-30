// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/cobalt_memory_context.h"
#include "cobalt/android/androidtv_flag_map.h"
#include "cobalt/android/jni_headers/JavaSwitches_jni.h"
#include "cobalt/app/cobalt_main_delegate.h"
#include "cobalt/app/cobalt_switch_defaults.h"
#include "content/public/app/content_jni_onload.h"
#include "content/public/app/content_main.h"

namespace {

// Takes the Android TV experiment switches handed over by Java
// (JavaSwitches.apply()) before the native library load.
cobalt::AndroidTvFlagMap TakeAndroidTvFlags() {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<std::string> key_values;
  base::android::AppendJavaStringArrayToStringVector(
      env, cobalt::Java_JavaSwitches_takeNativeKeyValues(env), &key_values);
  return cobalt::AndroidTvFlagMap::FromKeyValues(key_values);
}

// Applies Cobalt's command-line defaults right after LibraryLoader switches the
// CommandLine to native. LibraryLoader runs this hook once per process.
// Cobalt on Android runs in a single (browser) process (--single-process is a
// default), so the library is never loaded by a child process.
bool NativeInit(base::android::LibraryProcessType type) {
  CHECK_EQ(type, base::android::PROCESS_BROWSER);

  // Defaults must be applied before base::FeatureList is created.
  CHECK(!base::FeatureList::GetInstance());
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  // Expand Android TV experiment keys first so they take precedence over
  // defaults.
  cobalt::ExpandAndroidTvFlags(TakeAndroidTvFlags(), cmd_line);
  cobalt::CommandLinePreprocessor::ApplyDefaults(cmd_line);
  return true;
}

}  // namespace

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::memory::SetCurrentMemoryContext(
      base::memory::MemoryContext::kBrowserMain);
  base::android::InitVM(vm);
  if (!content::android::OnJNIOnLoadInit()) {
    return -1;
  }
  base::android::SetNativeInitializationHook(&NativeInit);
  content::SetContentMainDelegate(new cobalt::CobaltMainDelegate());
  return JNI_VERSION_1_4;
}
