// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cobalt_web_contents_observer.h"

#include "cobalt/embedded_resources/embedded_js.h"

#include "base/strings/utf_string_conversions.h"

namespace cobalt {

const char kEmbeddedJavascript[] = "test.js";

CobaltWebContentsObserver::CobaltWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  // Create browser-side mojo service component
  js_communication_host_ =
      std::make_unique<js_injection::JsCommunicationHost>(web_contents);

  // Get the embedded header resource
  GeneratedResourceMap resource_map;
  CobaltJavaScriptPolyfill::GenerateMap(resource_map);
  FileContents file_contents = resource_map[kEmbeddedJavascript];
  std::string js(reinterpret_cast<const char*>(file_contents.data),
                 file_contents.size);

  // Inject a script at document start for all origins
  const std::u16string script(base::UTF8ToUTF16(js));
  const std::vector<std::string> allowed_origins({"*"});
  auto result = js_communication_host_->AddDocumentStartJavaScript(
      script, allowed_origins);
  CHECK(!result.error_message);
}

// Placeholder for a WebContentsObserver override
void CobaltWebContentsObserver::PrimaryMainDocumentElementAvailable() {
  LOG(INFO) << "Cobalt::PrimaryMainDocumentElementAvailable";
}

}  // namespace cobalt
