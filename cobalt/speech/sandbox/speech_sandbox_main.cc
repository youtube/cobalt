// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "cobalt/base/wrap_main.h"
#include "cobalt/speech/sandbox/speech_sandbox.h"
#include "url/gurl.h"

namespace cobalt {
namespace speech {
namespace sandbox {

SpeechSandbox* g_speech_sandbox = NULL;

// The application takes an audio url or path, and a timeout in second.
// The timeout is optional. If it is not set or set to 0, the application
// doesn't shut down.
void StartApplication(int argc, char** argv, const char* link,
                      const base::Closure& quit_closure,
                      SbTimeMonotonic timestamp) {
  if (argc != 3 && argc != 2) {
    LOG(ERROR) << "Usage: " << argv[0]
               << " <audio url|path> [timeout in seconds]";
    return;
  }

  int timeout = 0;
  if (argc == 3) {
    base::StringToInt(argv[2], &timeout);
  }

  DCHECK(!g_speech_sandbox);
  g_speech_sandbox = new SpeechSandbox(
      std::string(argv[1]),
      base::FilePath(FILE_PATH_LITERAL("speech_sandbox_trace.json")));

  if (timeout != 0) {
    base::MessageLoop::current()->task_runner()->PostDelayedTask(
        FROM_HERE, quit_closure, base::TimeDelta::FromSeconds(timeout));
  }
}

void StopApplication() {
  DCHECK(g_speech_sandbox);
  delete g_speech_sandbox;
  g_speech_sandbox = NULL;
}

}  // namespace sandbox
}  // namespace speech
}  // namespace cobalt

COBALT_WRAP_BASE_MAIN(cobalt::speech::sandbox::StartApplication,
                      cobalt::speech::sandbox::StopApplication);
