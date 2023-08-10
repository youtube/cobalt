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

#ifndef COBALT_H5VCC_H5VCC_H_
#define COBALT_H5VCC_H5VCC_H_

#include <string>

#include "base/callback.h"
#include "cobalt/base/event_dispatcher.h"
#include "cobalt/dom/c_val_view.h"
#include "cobalt/dom/mutation_observer_task_manager.h"
#include "cobalt/h5vcc/h5vcc_accessibility.h"
#include "cobalt/h5vcc/h5vcc_audio_config_array.h"
#include "cobalt/h5vcc/h5vcc_crash_log.h"
#include "cobalt/h5vcc/h5vcc_metrics.h"
#include "cobalt/h5vcc/h5vcc_runtime.h"
#include "cobalt/h5vcc/h5vcc_settings.h"
#include "cobalt/h5vcc/h5vcc_storage.h"
#include "cobalt/h5vcc/h5vcc_system.h"
#include "cobalt/h5vcc/h5vcc_trace_event.h"
#include "cobalt/persistent_storage/persistent_settings.h"
#include "cobalt/script/wrappable.h"

#if SB_IS(EVERGREEN)
#include "cobalt/h5vcc/h5vcc_updater.h"
#include "cobalt/updater/updater_module.h"
#endif

namespace cobalt {
namespace h5vcc {

class H5vcc : public script::Wrappable {
 public:
  struct Settings {
    Settings()
        : media_module(NULL),
          can_play_type_handler(NULL),
          network_module(NULL),
#if SB_IS(EVERGREEN)
          updater_module(NULL),
#endif
          event_dispatcher(NULL),
          user_agent_data(NULL),
          global_environment(NULL) {
    }
    H5vccSettings::SetSettingFunc set_web_setting_func;
    media::MediaModule* media_module;
    media::CanPlayTypeHandler* can_play_type_handler;
    network::NetworkModule* network_module;
#if SB_IS(EVERGREEN)
    updater::UpdaterModule* updater_module;
#endif
    base::EventDispatcher* event_dispatcher;
    web::NavigatorUAData* user_agent_data;
    script::GlobalEnvironment* global_environment;
    persistent_storage::PersistentSettings* persistent_settings;
  };

  explicit H5vcc(const Settings& config);
  const scoped_refptr<H5vccAccessibility>& accessibility() const {
    return accessibility_;
  }
  const scoped_refptr<H5vccAudioConfigArray>& audio_config() {
    return audio_config_array_;
  }
  const scoped_refptr<dom::CValView>& c_val() const { return c_val_; }
  const scoped_refptr<H5vccCrashLog>& crash_log() const { return crash_log_; }
  const scoped_refptr<H5vccMetrics>& metrics() const { return metrics_; }
  const scoped_refptr<H5vccRuntime>& runtime() const { return runtime_; }
  const scoped_refptr<H5vccSettings>& settings() const { return settings_; }
  const scoped_refptr<H5vccStorage>& storage() const { return storage_; }
  const scoped_refptr<H5vccSystem>& system() const { return system_; }
  const scoped_refptr<H5vccTraceEvent>& trace_event() const {
    return trace_event_;
  }
#if SB_IS(EVERGREEN)
  const scoped_refptr<H5vccUpdater>& updater() const { return updater_; }
#endif

  DEFINE_WRAPPABLE_TYPE(H5vcc);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  scoped_refptr<H5vccAccessibility> accessibility_;
  scoped_refptr<H5vccAudioConfigArray> audio_config_array_;
  scoped_refptr<dom::CValView> c_val_;
  scoped_refptr<H5vccCrashLog> crash_log_;
  scoped_refptr<H5vccMetrics> metrics_;
  scoped_refptr<H5vccRuntime> runtime_;
  scoped_refptr<H5vccSettings> settings_;
  scoped_refptr<H5vccStorage> storage_;
  scoped_refptr<H5vccSystem> system_;
  scoped_refptr<H5vccTraceEvent> trace_event_;
#if SB_IS(EVERGREEN)
  scoped_refptr<H5vccUpdater> updater_;
#endif

  DISALLOW_COPY_AND_ASSIGN(H5vcc);
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_H_
