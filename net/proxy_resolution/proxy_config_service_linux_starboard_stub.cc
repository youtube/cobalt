// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "base/starboard/linker_stub.h"
#include "net/proxy_resolution/proxy_config_service_linux.h"

namespace net {

// TODO: b/413130400 - Cobalt: Resolve all the linker stubs
// Note: This is a stub implementation. The actual implementation is in
// proxy_config_service_linux.cc which is not included in the cobalt build.
// Stub symbols have been provided here to satisfy the linker and build cobalt.

void ProxyConfigServiceLinux::Delegate::SetUpAndFetchInitialConfig(
    const scoped_refptr<base::SingleThreadTaskRunner>& glib_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& main_task_runner,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  COBALT_LINKER_STUB();
}
}  // namespace net
