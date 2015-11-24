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

#ifndef H5VCC_H5VCC_H_
#define H5VCC_H5VCC_H_

#include "cobalt/dom/h5vcc_stub.h"
#include "cobalt/h5vcc/h5vcc_account_info.h"
#include "cobalt/h5vcc/h5vcc_audio_config_array.h"
#include "cobalt/h5vcc/h5vcc_storage.h"
#include "cobalt/h5vcc/h5vcc_system.h"

namespace cobalt {
namespace h5vcc {

class H5vcc : public dom::H5vccStub {
 public:
  struct Settings {
    Settings() : network_module(NULL), account_manager(NULL) {}
    network::NetworkModule* network_module;
    account::AccountManager* account_manager;
  };

  explicit H5vcc(const Settings& config);
  const scoped_refptr<H5vccAccountInfo>& account_info() const {
    return account_info_;
  }
  const scoped_refptr<H5vccAudioConfigArray>& audio_config() {
    return audio_config_array_;
  }
  const scoped_refptr<H5vccStorage>& storage() const { return storage_; }
  const scoped_refptr<H5vccSystem>& system() const { return system_; }

  DEFINE_WRAPPABLE_TYPE(H5vcc);

 private:
  scoped_refptr<H5vccAccountInfo> account_info_;
  scoped_refptr<H5vccAudioConfigArray> audio_config_array_;
  scoped_refptr<H5vccStorage> storage_;
  scoped_refptr<H5vccSystem> system_;

  DISALLOW_COPY_AND_ASSIGN(H5vcc);
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // H5VCC_H5VCC_H_
