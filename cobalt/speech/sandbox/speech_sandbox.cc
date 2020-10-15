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

#include "cobalt/speech/sandbox/speech_sandbox.h"

#include <memory>

#include "base/path_service.h"
#include "cobalt/dom/window.h"
#include "cobalt/script/environment_settings.h"

namespace cobalt {
namespace speech {
namespace sandbox {

namespace {
// The maximum number of element depth in the DOM tree. Elements at a level
// deeper than this could be discarded, and will not be rendered.
const int kDOMMaxElementDepth = 32;
}  // namespace

SpeechSandbox::SpeechSandbox(const std::string& file_path_string,
                             const base::FilePath& trace_log_path) {
  trace_to_file_.reset(new trace_event::ScopedTraceToFile(trace_log_path));

  network::NetworkModule::Options network_options;
  network_options.https_requirement = network::kHTTPSOptional;
  network_module_.reset(new network::NetworkModule(network_options));

  GURL url(file_path_string);
  if (url.is_valid()) {
    audio_loader_.reset(new AudioLoader(
        url, network_module_.get(),
        base::Bind(&SpeechSandbox::OnLoadingDone, base::Unretained(this))));
  } else {
    base::FilePath file_path(file_path_string);
    if (!file_path.IsAbsolute()) {
      base::FilePath exe_path;
      base::PathService::Get(base::FILE_EXE, &exe_path);
      DCHECK(exe_path.IsAbsolute());
      std::string exe_path_string(exe_path.value());
      std::size_t found = exe_path_string.find_last_of("/\\");
      DCHECK_NE(found, std::string::npos);
      // Find the executable directory. Using exe_path.DirName() doesn't work
      // on Windows based platforms due to the path is mixed with "/" and "\".
      exe_path_string = exe_path_string.substr(0, found);
      file_path = base::FilePath(exe_path_string).Append(file_path_string);
      DCHECK(file_path.IsAbsolute());
    }

    dom::DOMSettings::Options dom_settings_options;
    dom_settings_options.microphone_options.enable_fake_microphone = true;
    dom_settings_options.microphone_options.file_path = file_path;

    StartRecognition(dom_settings_options);
  }
}

SpeechSandbox::~SpeechSandbox() {
  if (speech_recognition_) {
    speech_recognition_->Stop();
  }
}

void SpeechSandbox::StartRecognition(
    const dom::DOMSettings::Options& dom_settings_options) {
  std::unique_ptr<script::EnvironmentSettings> environment_settings(
      new dom::DOMSettings(kDOMMaxElementDepth, NULL, network_module_.get(),
                           NULL, NULL, NULL, NULL, NULL, NULL,
                           null_debugger_hooks_, NULL, dom_settings_options));
  DCHECK(environment_settings);

  speech_recognition_ = new SpeechRecognition(environment_settings.get());
  speech_recognition_->set_continuous(true);
  speech_recognition_->set_interim_results(true);
  speech_recognition_->Start(NULL);
}

void SpeechSandbox::OnLoadingDone(const uint8* data, int size) {
  dom::DOMSettings::Options dom_settings_options;
  dom_settings_options.microphone_options.enable_fake_microphone = true;
  dom_settings_options.microphone_options.external_audio_data = data;
  dom_settings_options.microphone_options.audio_data_size = size;

  StartRecognition(dom_settings_options);
}

}  // namespace sandbox
}  // namespace speech
}  // namespace cobalt
