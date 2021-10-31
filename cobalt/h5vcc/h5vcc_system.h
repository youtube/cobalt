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

#ifndef COBALT_H5VCC_H5VCC_SYSTEM_H_
#define COBALT_H5VCC_H5VCC_SYSTEM_H_

#include <string>

#include "starboard/configuration.h"
#if SB_IS(EVERGREEN)
#include "cobalt/h5vcc/h5vcc_updater.h"
#endif
#include "cobalt/media/media_module.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace h5vcc {

class H5vccSystem : public script::Wrappable {
 public:
#if SB_IS(EVERGREEN)
  explicit H5vccSystem(H5vccUpdater* updater);
#else
  H5vccSystem();
#endif

  bool are_keys_reversed() const;
  std::string build_id() const;
  std::string platform() const;
  std::string region() const;
  std::string version() const;

  bool TriggerHelp() const;

  enum UserOnExitStrategy {
    kUserOnExitStrategyClose,
    kUserOnExitStrategyMinimize,
    kUserOnExitStrategyNoExit,
  };

  uint32 user_on_exit_strategy() const;

  DEFINE_WRAPPABLE_TYPE(H5vccSystem);

 private:
  std::string video_container_size_;
#if SB_IS(EVERGREEN)
  scoped_refptr<H5vccUpdater> updater_;
#endif
  DISALLOW_COPY_AND_ASSIGN(H5vccSystem);
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_SYSTEM_H_
