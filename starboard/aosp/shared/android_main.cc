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

#include <string>
#include <vector>

#include "base/threading/platform_thread.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/log.h"
#include "starboard/shared/starboard/features.h"
#include "third_party/jni_zero/jni_zero.h"

int main(int argc, char** argv);

namespace {

class StarboardMainDelegate : public base::PlatformThread::Delegate {
 public:
  void ThreadMain() override {
    base::PlatformThread::SetName("StarboardMain");

    JNIEnv* env = jni_zero::AttachCurrentThread();
    std::vector<std::string> args;
    args.push_back("cobalt_loader");
    starboard::StarboardBridge::GetInstance()->AppendArgs(env, &args);

    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (std::string& arg : args) {
      argv.push_back(arg.data());
    }
    argv.push_back(nullptr);

    // Features might be queried during static initialization of the library. To
    // avoid a crash, initialize them before the loader app does its work.
    starboard::features::InitializeStarboardFeatureListWithDefaults();

    SB_LOG(INFO) << "cobalt_loader: entering main() with " << args.size()
                 << " args";
    int error_level = main(static_cast<int>(args.size()), argv.data());
    SB_LOG(INFO) << "cobalt_loader: main() returned " << error_level;
  }
};

}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_dev_cobalt_app_MainActivity_nativeStartStarboard(JNIEnv*, jclass) {
  SB_LOG(INFO) << "cobalt_loader: nativeStartStarboard entered";

  static StarboardMainDelegate delegate;
  base::PlatformThread::CreateNonJoinable(0, &delegate);
}
