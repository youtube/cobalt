// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/updater/updater.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool/initialization_util.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "cobalt/network/network_module.h"
#include "cobalt/updater/configurator.h"
#include "cobalt/updater/updater_module.h"
#include "components/crx_file/crx_verifier.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "starboard/event.h"

namespace {

void TaskSchedulerStart() {
  base::ThreadPoolInstance::Create("Updater");
  const auto task_scheduler_init_params =
      std::make_unique<base::ThreadPoolInstance::InitParams>(
          base::RecommendedMaxNumberOfThreadsInThreadGroup(3, 8, 0.1, 0),
          base::RecommendedMaxNumberOfThreadsInThreadGroup(8, 32, 0.3, 0));
  base::ThreadPoolInstance::Get()->Start(*task_scheduler_init_params);
}

void TaskSchedulerStop() { base::ThreadPoolInstance::Get()->Shutdown(); }

std::unique_ptr<base::SingleThreadTaskExecutor> g_loop;
std::unique_ptr<cobalt::network::NetworkModule> network_module;
std::unique_ptr<base::AtExitManager> exit_manager;
std::unique_ptr<cobalt::updater::UpdaterModule> updater_module;

}  // namespace

namespace cobalt {
namespace updater {

int UpdaterMain(int argc, const char* const* argv) {
  exit_manager.reset(new base::AtExitManager());

  base::CommandLine::Init(argc, argv);

  TaskSchedulerStart();

  g_loop.reset(new base::SingleThreadTaskExecutor(base::MessagePumpType::UI));

#ifndef COBALT_PENDING_CLEAN_UP
  DCHECK(base::ThreadTaskRunnerHandle::IsSet());
#endif
  base::PlatformThread::SetName("UpdaterMain");

  network::NetworkModule::Options network_options;
  network_module.reset(new network::NetworkModule(network_options));

  updater_module.reset(new updater::UpdaterModule(
      network_module.get(), kDefaultUpdateCheckDelaySeconds));

  return 0;
}

}  // namespace updater
}  // namespace cobalt
