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

#ifndef COBALT_H5VCC_H5VCC_H_
#define COBALT_H5VCC_H5VCC_H_

#include <string>

#include "cobalt/base/event_dispatcher.h"
#include "cobalt/dom/mutation_observer_task_manager.h"
#include "cobalt/dom/window.h"
#include "cobalt/h5vcc/h5vcc_accessibility.h"
#include "cobalt/h5vcc/h5vcc_account_info.h"
#include "cobalt/h5vcc/h5vcc_audio_config_array.h"
#include "cobalt/h5vcc/h5vcc_c_val.h"
#include "cobalt/h5vcc/h5vcc_crash_log.h"
#include "cobalt/h5vcc/h5vcc_runtime.h"
#include "cobalt/h5vcc/h5vcc_settings.h"
#include "cobalt/h5vcc/h5vcc_sso.h"
#include "cobalt/h5vcc/h5vcc_storage.h"
#include "cobalt/h5vcc/h5vcc_system.h"
#include "cobalt/h5vcc/h5vcc_trace_event.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace h5vcc {

class H5vcc : public script::Wrappable {
 public:
  struct Settings {
    Settings()
        : media_module(NULL),
          network_module(NULL),
          account_manager(NULL),
          event_dispatcher(NULL) {}
    media::MediaModule* media_module;
    network::NetworkModule* network_module;
    account::AccountManager* account_manager;
    base::EventDispatcher* event_dispatcher;
    std::string initial_deep_link;
  };

  H5vcc(const Settings& config, const scoped_refptr<dom::Window>& window,
        dom::MutationObserverTaskManager* mutation_observer_task_manager);
  const scoped_refptr<H5vccAccessibility>& accessibility() const {
    return accessibility_;
  }
  const scoped_refptr<H5vccAccountInfo>& account_info() const {
    return account_info_;
  }
  const scoped_refptr<H5vccAudioConfigArray>& audio_config() {
    return audio_config_array_;
  }
  const scoped_refptr<H5vccCVal>& c_val() const { return c_val_; }
  const scoped_refptr<H5vccCrashLog>& crash_log() const { return crash_log_; }
  const scoped_refptr<H5vccRuntime>& runtime() const { return runtime_; }
  const scoped_refptr<H5vccSettings>& settings() const { return settings_; }
#if defined(COBALT_ENABLE_SSO)
  const scoped_refptr<H5vccSso>& sso() const { return sso_; }
#endif
  const scoped_refptr<H5vccStorage>& storage() const { return storage_; }
  const scoped_refptr<H5vccSystem>& system() const { return system_; }
  const scoped_refptr<H5vccTraceEvent>& trace_event() const {
    return trace_event_;
  }

  DEFINE_WRAPPABLE_TYPE(H5vcc);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  scoped_refptr<H5vccAccessibility> accessibility_;
  scoped_refptr<H5vccAccountInfo> account_info_;
  scoped_refptr<H5vccAudioConfigArray> audio_config_array_;
  scoped_refptr<H5vccCVal> c_val_;
  scoped_refptr<H5vccCrashLog> crash_log_;
  scoped_refptr<H5vccRuntime> runtime_;
  scoped_refptr<H5vccSettings> settings_;
  scoped_refptr<H5vccSso> sso_;
  scoped_refptr<H5vccStorage> storage_;
  scoped_refptr<H5vccSystem> system_;
  scoped_refptr<H5vccTraceEvent> trace_event_;

  DISALLOW_COPY_AND_ASSIGN(H5vcc);
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_H_
