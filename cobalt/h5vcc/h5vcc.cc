// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/sso/sso_interface.h"

namespace cobalt {
namespace h5vcc {

H5vcc::H5vcc(const Settings& settings, const scoped_refptr<dom::Window>& window,
             dom::MutationObserverTaskManager* mutation_observer_task_manager) {
  accessibility_ = new H5vccAccessibility(settings.event_dispatcher, window,
                                          mutation_observer_task_manager);
  account_info_ = new H5vccAccountInfo(settings.account_manager);
  audio_config_array_ = new H5vccAudioConfigArray();
  c_val_ = new H5vccCVal();
  crash_log_ = new H5vccCrashLog();
  runtime_ =
      new H5vccRuntime(settings.event_dispatcher, settings.initial_deep_link);
  settings_ = new H5vccSettings(settings.media_module);
#if defined(COBALT_ENABLE_SSO)
  sso_ = new H5vccSso();
#endif
  storage_ = new H5vccStorage(settings.network_module);
  system_ = new H5vccSystem();
  trace_event_ = new H5vccTraceEvent();
}

void H5vcc::TraceMembers(script::Tracer* tracer) {
  tracer->Trace(accessibility_);
  tracer->Trace(account_info_);
  tracer->Trace(audio_config_array_);
  tracer->Trace(c_val_);
  tracer->Trace(crash_log_);
  tracer->Trace(runtime_);
  tracer->Trace(settings_);
  tracer->Trace(sso_);
  tracer->Trace(storage_);
  tracer->Trace(system_);
  tracer->Trace(trace_event_);
}

}  // namespace h5vcc
}  // namespace cobalt
