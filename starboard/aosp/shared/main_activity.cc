// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "cobalt/aosp/jni_headers/MainActivity_jni.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/android/shared/window_surface.h"
#include "starboard/common/log.h"
#include "starboard/system.h"
#include "third_party/jni_zero/jni_zero.h"

int main(int argc, char** argv);

namespace {

// Cobalt normally runs main() on the process's main thread, which has a large
// (typically 8 MB) stack. Here it runs on a dedicated thread instead, and
// bionic's default thread stack is only ~1 MB.
constexpr size_t kStarboardMainStackSize = 8 * 1024 * 1024;

void* StarboardMain(void* /*context*/) {
  pthread_setname_np(pthread_self(), "StarboardMain");

  JNIEnv* env = jni_zero::AttachCurrentThread();
  // Android starts the process with the working directory at "/" (read-only)
  // nplb (and POSIX code) may expect relative paths to be writable, so
  // cd to a writable app directory before startup.
  char files_dir[PATH_MAX];
  if (SbSystemGetPath(kSbSystemPathFilesDirectory, files_dir,
                      sizeof(files_dir))) {
    if (chdir(files_dir) != 0) {
      SB_LOG(WARNING) << "cobalt_loader: chdir to " << files_dir << " failed";
    }
    // point TMPDIR at the writable app files directory
    setenv("TMPDIR", files_dir, 1);
  }

  std::vector<std::string> args;
  args.push_back("cobalt_loader");
  // Don't use "/dev/shm" for shared memory; it does not exist on Android
  // With this switch it falls back to GetTempDir()
  args.push_back("--disable-dev-shm-usage");
  starboard::StarboardBridge::GetInstance()->AppendArgs(env, &args);

  std::vector<char*> argv;
  argv.reserve(args.size() + 1);
  for (std::string& arg : args) {
    argv.push_back(arg.data());
  }
  argv.push_back(nullptr);

  main(static_cast<int>(args.size()), argv.data());
  return nullptr;
}

}  // namespace

namespace starboard {

void JNI_MainActivity_StartLoader(JNIEnv* env) {
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, kStarboardMainStackSize);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  pthread_t thread;
  if (pthread_create(&thread, &attr, &StarboardMain, nullptr) != 0) {
    SB_LOG(ERROR) << "Failed to create StarboardMain thread";
  }

  pthread_attr_destroy(&attr);
}

// The Android graphics SurfaceView hands its Surface to Starboard here. The app
// only calls nativeStartStarboard() after surfaceCreated, so by the time the
// inner library's Ozone window asks for it via SbWindowCreate() the
// ANativeWindow is already published and the create is synchronous.
void JNI_MainActivity_NativeOnSurfaceCreated(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jobject>& surface) {
  ANativeWindow* native_window = ANativeWindow_fromSurface(env, surface.obj());
  SB_LOG(INFO) << "cobalt_loader: Starboard surface created, native_window="
               << native_window;
  starboard::android::shared::SetWindowSurface(native_window);
}

void JNI_MainActivity_NativeOnSurfaceDestroyed(JNIEnv*) {
  SB_LOG(INFO) << "cobalt_loader: Starboard surface destroyed.";
  starboard::android::shared::SetWindowSurface(nullptr);
}

}  // namespace starboard
