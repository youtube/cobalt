// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_LINUX_SYSTEM_NETWORK_STATUS_H_
#define STARBOARD_SHARED_LINUX_SYSTEM_NETWORK_STATUS_H_

#if SB_API_VERSION >= 13

#include "starboard/shared/linux/singleton.h"
#include "starboard/system.h"
#include "starboard/thread.h"

class NetworkNotifier : public starboard::Singleton<NetworkNotifier> {
 public:
  bool Initialize();

  static void* NotifierThreadEntry(void* context);

  bool is_online() { return is_online_; }
  void set_online(bool is_online) { is_online_ = is_online; }

 private:
  SbThread notifier_thread_;
  bool is_online_ = true;
};

#endif  // SB_API_VERSION >= 13

#endif  // STARBOARD_SHARED_LINUX_SYSTEM_NETWORK_STATUS_H_
