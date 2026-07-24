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

#include <jni.h>
#include <limits.h>
#include <pthread.h>
#include <unistd.h>

#include <string>
#include <thread>
#include <vector>

#include "cobalt/aosp/jni_headers/MainActivity_jni.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/log.h"
#include "starboard/system.h"
#include "third_party/jni_zero/jni_zero.h"

int main(int argc, char** argv);

namespace {

void StarboardMain() {
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
  }

  std::vector<std::string> args;
  args.push_back("cobalt_loader");
  starboard::StarboardBridge::GetInstance()->AppendArgs(env, &args);

  std::vector<char*> argv;
  argv.reserve(args.size() + 1);
  for (std::string& arg : args) {
    argv.push_back(arg.data());
  }
  argv.push_back(nullptr);

  main(static_cast<int>(args.size()), argv.data());
}

}  // namespace

namespace starboard {

void JNI_MainActivity_StartLoader(JNIEnv* env) {
  std::thread(StarboardMain).detach();
}

}  // namespace starboard
