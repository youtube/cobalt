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

#include "cobalt/browser/cobalt_web_contents_observer.h"

#include "base/strings/utf_string_conversions.h"
#include "cobalt/browser/embedded_resources/embedded_js.h"
#include "cobalt/browser/migrate_storage_record/migration_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROIDTV)
#include "cobalt/android/oom_intervention/oom_intervention_tab_helper.h"
#include "starboard/android/shared/starboard_bridge.h"

using starboard::android::shared::StarboardBridge;
#endif

namespace cobalt {

CobaltWebContentsObserver::CobaltWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  // Create browser-side mojo service component
  js_communication_host_ =
      std::make_unique<js_injection::JsCommunicationHost>(web_contents);

  RegisterInjectedJavaScript();

#if BUILDFLAG(IS_ANDROIDTV)
  if (OomInterventionTabHelper::IsEnabled()) {
    OomInterventionTabHelper::CreateForWebContents(web_contents);
  }
#endif
}

void CobaltWebContentsObserver::RegisterInjectedJavaScript() {
  // Get the embedded header resource
  GeneratedResourceMap resource_map;
  CobaltJavaScriptPolyfill::GenerateMap(resource_map);

  for (const auto& [file_name, file_contents] : resource_map) {
    LOG(INFO) << "JS injection for filename: " << file_name;
    std::string js(reinterpret_cast<const char*>(file_contents.data),
                   file_contents.size);

    // Inject a script at document start for all origins
    const std::u16string script(base::UTF8ToUTF16(js));
    const std::vector<std::string> allowed_origins({"*"});
    auto result = js_communication_host_->AddDocumentStartJavaScript(
        script, allowed_origins);

    if (result.error_message.has_value()) {
      // error_message contains a value
      LOG(WARNING) << "Failed to register JS injection for:" << file_name
                   << ", error message: " << result.error_message.value();
    }
  }
}

void CobaltWebContentsObserver::PrimaryMainDocumentElementAvailable() {
  migrate_storage_record::MigrationManager::DoMigrationTasksOnce(
      web_contents());
}

void CobaltWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  LOG(INFO) << "Navigated to: " << navigation_handle->GetURL();
}

}  // namespace cobalt
