// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/h5vcc/h5vcc.h"

#if defined(COBALT_ENABLE_ACCOUNT_MANAGER)
#include "cobalt/h5vcc/h5vcc_account_manager_internal.h"
#include "cobalt/script/source_code.h"
#endif

#include "cobalt/persistent_storage/persistent_settings.h"

namespace cobalt {
namespace h5vcc {

H5vcc::H5vcc(const Settings& settings) {
  accessibility_ = new H5vccAccessibility(settings.event_dispatcher);
  account_info_ = new H5vccAccountInfo(settings.account_manager);
  audio_config_array_ = new H5vccAudioConfigArray();
  c_val_ = new dom::CValView();
  crash_log_ = new H5vccCrashLog();
  runtime_ = new H5vccRuntime(settings.event_dispatcher);
  settings_ =
      new H5vccSettings(settings.set_web_setting_func, settings.media_module,
                        settings.network_module,
#if SB_IS(EVERGREEN)
                        settings.updater_module,
#endif
                        settings.user_agent_data, settings.global_environment);
  storage_ =
      new H5vccStorage(settings.network_module, settings.persistent_settings);
  trace_event_ = new H5vccTraceEvent();
#if SB_IS(EVERGREEN)
  updater_ = new H5vccUpdater(settings.updater_module);
  system_ = new H5vccSystem(updater_);
#else
  system_ = new H5vccSystem();
#endif

#if defined(COBALT_ENABLE_ACCOUNT_MANAGER)
  // Bind "H5vccAccountManager" if it is supported. (This is not to be confused
  // with settings.account_manager.)
  if (H5vccAccountManagerInternal::IsSupported()) {
    // Since we don't want to bind an instance of a wrappable, we cannot use
    // Bind() nor BindTo(). Instead, just evaluate a script to alias the type.
    scoped_refptr<script::SourceCode> source =
        script::SourceCode::CreateSourceCode(
            "H5vccAccountManager = H5vccAccountManagerInternal;"
            "window.H5vccAccountManager = window.H5vccAccountManagerInternal;",
            base::SourceLocation("h5vcc.cc", __LINE__, 1));
    std::string result;
    bool success = settings.global_environment->EvaluateScript(source, &result);
    CHECK(success);
  }
#endif
}

void H5vcc::TraceMembers(script::Tracer* tracer) {
  tracer->Trace(accessibility_);
  tracer->Trace(account_info_);
  tracer->Trace(audio_config_array_);
  tracer->Trace(c_val_);
  tracer->Trace(crash_log_);
  tracer->Trace(runtime_);
  tracer->Trace(settings_);
  tracer->Trace(storage_);
  tracer->Trace(system_);
  tracer->Trace(trace_event_);
#if SB_IS(EVERGREEN)
  tracer->Trace(updater_);
#endif
}

}  // namespace h5vcc
}  // namespace cobalt
