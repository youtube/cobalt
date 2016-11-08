// Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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

#include "starboard/configuration.h"
#include "third_party/starboard/android/shared/application_android.h"
#include "third_party/starboard/android/shared/exports.h"

#include <errno.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/native_window.h>
#include <android/log.h>
#include <unistd.h>
#include "starboard/queue.h"

/* For executable
/int main(int argc, char** argv) {
  starboard::android::shared::ApplicationAndroid application;
  return application.Run(argc, argv);
}*/

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "native-activity", __VA_ARGS__))

starboard::Queue<AInputEvent*> sharedEventQueue;


void starboard_queue_input_event(AInputEvent* event) {
    sharedEventQueue.Put(event);
}

int starboard_initialize(void* args) {
  struct args *sb_args = (struct args *)args;

  starboard::android::shared::ApplicationAndroid application;
  starboard::android::shared::ApplicationAndroid::Get()->SetWindowHandle(sb_args->main_window);
  starboard::android::shared::ApplicationAndroid::Get()->SetVideoWindowHandle(sb_args->video_window);
  starboard::android::shared::ApplicationAndroid::Get()->SetExternalFilesDir(sb_args->external_files_dir);
  starboard::android::shared::ApplicationAndroid::Get()->SetExternalCacheDir(sb_args->external_cache_dir);

  starboard::android::shared::ApplicationAndroid::Get()->SetMessageFDs(sb_args->sbToAppMsgRead, sb_args->sbToAppMsgWrite);

  int32_t w = ANativeWindow_getWidth(sb_args->main_window);
  int32_t h = ANativeWindow_getHeight(sb_args->main_window);
  int32_t w2 = ANativeWindow_getWidth(sb_args->video_window);
  int32_t h2 = ANativeWindow_getHeight(sb_args->video_window);

  LOGI("EGL window -> width = %d, height = %d; video -> width = %d, height = %d\n", w, h);

  int argc = 2;
  char argv0[] = "./nplb";
  char argv1[] = "--gtest_filter=SbWindow*";
  char *argv[2];
  argv[0] = argv0;
  argv[1] = argv1;
  return application.Run(argc, argv);
}
