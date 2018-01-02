// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "starboard/android/shared/application_android.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/log_file_impl.h"
#include "starboard/common/semaphore.h"
#include "starboard/shared/starboard/command_line.h"
#include "starboard/string.h"
#include "starboard/thread.h"

namespace starboard {
namespace android {
namespace shared {
namespace {

using ::starboard::shared::starboard::CommandLine;
typedef ::starboard::android::shared::ApplicationAndroid::AndroidCommand
    AndroidCommand;

const char kLogPathSwitch[] = "android_log_file";

SbThread g_starboard_thread = kSbThreadInvalid;

std::vector<char*> GetArgs() {
  std::vector<char*> args;
  // Fake program name as args[0]
  args.push_back(SbStringDuplicate("android_main"));

  JniEnvExt* env = JniEnvExt::Get();

  ScopedLocalJavaRef<jobjectArray> args_array(
      env->CallStarboardObjectMethodOrAbort("getArgs",
                                            "()[Ljava/lang/String;"));
  jint argc = !args_array ? 0 : env->GetArrayLength(args_array.Get());

  for (jint i = 0; i < argc; i++) {
    ScopedLocalJavaRef<jstring> element(
        env->GetObjectArrayElementOrAbort(args_array.Get(), i));
    std::string utf_str = env->GetStringStandardUTFOrAbort(element.Get());
    args.push_back(SbStringDuplicate(utf_str.c_str()));
  }

  return args;
}

std::string GetStartDeepLink() {
  JniEnvExt* env = JniEnvExt::Get();
  std::string start_url;

  ScopedLocalJavaRef<jstring> j_url(env->CallStarboardObjectMethodOrAbort(
      "getStartDeepLink", "()Ljava/lang/String;"));
  if (j_url) {
    start_url = env->GetStringStandardUTFOrAbort(j_url.Get());
  }
  return start_url;
}

// Work around JNIEnv::AttachCurrentThread() renaming our thread.
void FixThreadName() {
  char thread_name[16];
  SbThreadGetName(thread_name, sizeof(thread_name));
  JniEnvExt::Get();
  SbThreadSetName(thread_name);
}

void* ThreadEntryPoint(void* context) {
  Semaphore* app_created_semaphore = static_cast<Semaphore*>(context);
  FixThreadName();

  ALooper* looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);

  ApplicationAndroid app(looper);

  // TODO: Implement move semantics in CommandLine and use it directly.
  std::vector<char*> args(GetArgs());
  std::string start_url(GetStartDeepLink());

  {
    CommandLine cl(args.size(), args.data());
    if (cl.HasSwitch(kLogPathSwitch)) {
      OpenLogFile(cl.GetSwitchValue(kLogPathSwitch).c_str());
    }
  }

  // Signal ANativeActivity_onCreate() that it may proceed.
  app_created_semaphore->Put();

  app.Run(args.size(), args.data());

  for (std::vector<char*>::iterator it = args.begin(); it != args.end(); ++it) {
    SbMemoryDeallocate(*it);
  }

  return NULL;
}

void OnStart(ANativeActivity* activity) {
  ApplicationAndroid::Get()->SendAndroidCommand(AndroidCommand::kStart);
}

void OnResume(ANativeActivity* activity) {
  ApplicationAndroid::Get()->SendAndroidCommand(AndroidCommand::kResume);
}

void OnPause(ANativeActivity* activity) {
  ApplicationAndroid::Get()->SendAndroidCommand(AndroidCommand::kPause);
}

void OnStop(ANativeActivity* activity) {
  ApplicationAndroid::Get()->SendAndroidCommand(AndroidCommand::kStop);
}

void OnDestroy(ANativeActivity* activity) {
  ApplicationAndroid::Get()->SendAndroidCommand(AndroidCommand::kDestroy);
}

void OnWindowFocusChanged(ANativeActivity* activity, int focused) {
  ApplicationAndroid::Get()->SendAndroidCommand(focused
      ? AndroidCommand::kWindowFocusGained
      : AndroidCommand::kWindowFocusLost);
}

void OnNativeWindowCreated(ANativeActivity* activity, ANativeWindow* window) {
  ApplicationAndroid::Get()->SendAndroidCommand(
      AndroidCommand::kNativeWindowCreated, window);
}

void OnNativeWindowDestroyed(ANativeActivity* activity, ANativeWindow* window) {
  ApplicationAndroid::Get()->SendAndroidCommand(
      AndroidCommand::kNativeWindowDestroyed);
}

void OnInputQueueCreated(ANativeActivity* activity, AInputQueue* queue) {
  ApplicationAndroid::Get()->SendAndroidCommand(
      AndroidCommand::kInputQueueChanged, queue);
}

void OnInputQueueDestroyed(ANativeActivity* activity, AInputQueue* queue) {
  ApplicationAndroid::Get()->SendAndroidCommand(
      AndroidCommand::kInputQueueChanged, NULL);
}

extern "C" SB_EXPORT_PLATFORM void ANativeActivity_onCreate(
    ANativeActivity *activity, void *savedState, size_t savedStateSize) {
  SB_DLOG(INFO) << "ANativeActivity_onCreate";

  // Start the Starboard thread the first time an Activity is created.
  if (!SbThreadIsValid(g_starboard_thread)) {
    Semaphore semaphore;

    g_starboard_thread = SbThreadCreate(
        0, kSbThreadPriorityNormal, kSbThreadNoAffinity, false,
        "StarboardMain", &ThreadEntryPoint, &semaphore);

    // Wait for the ApplicationAndroid to be created.
    semaphore.Take();
  }

  activity->callbacks->onStart = OnStart;
  activity->callbacks->onResume = OnResume;
  activity->callbacks->onPause = OnPause;
  activity->callbacks->onStop = OnStop;
  activity->callbacks->onDestroy = OnDestroy;
  activity->callbacks->onWindowFocusChanged = OnWindowFocusChanged;
  activity->callbacks->onNativeWindowCreated = OnNativeWindowCreated;
  activity->callbacks->onNativeWindowDestroyed = OnNativeWindowDestroyed;
  activity->callbacks->onInputQueueCreated = OnInputQueueCreated;
  activity->callbacks->onInputQueueDestroyed = OnInputQueueDestroyed;
  activity->instance = ApplicationAndroid::Get();
}

extern "C" SB_EXPORT_PLATFORM
jboolean Java_foo_cobalt_coat_StarboardBridge_nativeReleaseBuild() {
#if defined(COBALT_BUILD_TYPE_GOLD)
  return true;
#else
  return false;
#endif
}

extern "C" SB_EXPORT_PLATFORM
void Java_foo_cobalt_coat_StarboardBridge_nativeInitialize(
    JniEnvExt* env, jobject starboard_bridge) {
  JniEnvExt::Initialize(env, starboard_bridge);
}

}  // namespace
}  // namespace shared
}  // namespace android
}  // namespace starboard
