// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/system.h"

#include <android/native_activity.h>
#include <stdio.h>
#include <string>

#include "starboard/android/shared/application_android.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/log_file_impl.h"
#include "starboard/file.h"
#include "starboard/shared/starboard/command_line.h"

using starboard::android::shared::ApplicationAndroid;
using starboard::shared::starboard::CommandLine;
using starboard::android::shared::CloseLogFile;
using starboard::android::shared::JniEnvExt;

namespace {

const char kExitFilePathSwitch[] = "android_exit_file";

}

void SbSystemRequestStop(int error_level) {
  ApplicationAndroid* app = ApplicationAndroid::Get();
  app->SetExitOnActivityDestroy(error_level);

  const CommandLine* cl = app->GetCommandLine();

  std::string path = cl->GetSwitchValue(kExitFilePathSwitch);

  if (!path.empty()) {
    // Since we cannot reliably flush the exit code in the log,
    // we write a file to our app files directory.

    // We are going to make a temporary file here and rename it
    // to the actual exitcode file.  This is so that tools searching for
    // the exitcode file don't find it until it has been written to.
    std::string temp_path = path;
    temp_path.append(".tmp");

    SbFile exitcode_file = SbFileOpen(
      temp_path.c_str(),
      kSbFileCreateAlways | kSbFileRead | kSbFileWrite, NULL, NULL);

    if (exitcode_file != NULL) {
      std::ostringstream exit_stream;
      exit_stream << std::dec << error_level;

      SbFileWrite(exitcode_file, exit_stream.str().c_str(),
                  exit_stream.str().size());
      SbFileClose(exitcode_file);
      rename(temp_path.c_str(), path.c_str());
    }
  }

  CloseLogFile();

  JniEnvExt* env = JniEnvExt::Get();
  env->CallStarboardVoidMethodOrAbort("requestStop", "()V");
}
