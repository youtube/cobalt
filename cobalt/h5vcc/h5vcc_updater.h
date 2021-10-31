// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

// Note: this header will be used to generate bindings for the H5vccUpdater
// interface even on non-evergreen platforms where this header is not included
// in the h5vcc.gyp file. But eventually the updater attribute/interface will
// not be present in the window.H5vcc interface on non-evergreen platforms.

#ifndef COBALT_H5VCC_H5VCC_UPDATER_H_
#define COBALT_H5VCC_H5VCC_UPDATER_H_

#include <string>

#include "cobalt/script/wrappable.h"

#if SB_IS(EVERGREEN)
#include "cobalt/updater/updater_module.h"
#endif

namespace cobalt {
namespace h5vcc {

class H5vccUpdater : public script::Wrappable {
 public:
#if SB_IS(EVERGREEN)
  explicit H5vccUpdater(updater::UpdaterModule* updater_module)
      : updater_module_(updater_module) {}

  std::string GetUpdaterChannel() const;
  void SetUpdaterChannel(const std::string& channel);

  std::string GetUpdateStatus() const;

  void ResetInstallations();

  uint16 GetInstallationIndex() const;

#else
  H5vccUpdater() {}
#endif
  DEFINE_WRAPPABLE_TYPE(H5vccUpdater);

 private:
#if SB_IS(EVERGREEN)
  updater::UpdaterModule* updater_module_;
#endif
  DISALLOW_COPY_AND_ASSIGN(H5vccUpdater);
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_UPDATER_H_
