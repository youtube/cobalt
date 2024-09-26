// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/init/ios_global_state_configuration.h"
#include "base/task/single_thread_task_runner.h"

namespace ios_global_state {

scoped_refptr<base::SingleThreadTaskRunner>
GetSharedNetworkIOThreadTaskRunner() {
  return nullptr;
}

}  // namespace ios_global_state
