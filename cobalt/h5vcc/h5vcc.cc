/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/h5vcc/h5vcc.h"

namespace cobalt {
namespace h5vcc {

H5vcc::H5vcc(const Settings& settings) {
  account_info_ = new H5vccAccountInfo(settings.account_manager);
  audio_config_array_ = new H5vccAudioConfigArray();
  c_val_ = new H5vccCVal();
  runtime_ =
      new H5vccRuntime(settings.event_dispatcher, settings.initial_deep_link);
  settings_ = new H5vccSettings(settings.media_module);
  storage_ = new H5vccStorage(settings.network_module);
  system_ = new H5vccSystem(settings.on_set_record_stats);
}

}  // namespace h5vcc
}  // namespace cobalt
