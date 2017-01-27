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

#include "starboard/android/shared/application_android.h"
#include "starboard/file.h"
#include "starboard/shared/starboard/command_line.h"

using starboard::android::shared::ApplicationAndroid;
using starboard::shared::starboard::CommandLine;

namespace {
const char kExitFilePathSwitch[] = "android_exit_file";
}

void SbSystemRequestStop(int error_level) {
  ApplicationAndroid* app = ApplicationAndroid::Get();
  app->SetExitOnActivityDestroy(error_level);

  CommandLine* cl = app->GetCommandLine();

  std::string path = cl->GetSwitchValue(kExitFilePathSwitch);

  if (!path.empty()) {
    // Since we cannot reliably flush the exit code in the log,
    // we write a file to our app files directory.

    SbFile exitcode_file = SbFileOpen(
      path.c_str(),
      kSbFileCreateAlways | kSbFileRead | kSbFileWrite, NULL, NULL);

    if (exitcode_file != NULL) {
      std::ostringstream exit_stream;
      exit_stream << std::dec << error_level;

      SbFileWrite(exitcode_file, exit_stream.str().c_str(),
                  exit_stream.str().size());
      SbFileClose(exitcode_file);
    }
  }

  ANativeActivity_finish(ApplicationAndroid::Get()->GetActivity());
}
