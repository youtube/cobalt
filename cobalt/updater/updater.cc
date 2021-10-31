// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/updater/updater.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/task_scheduler/initialization_util.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/task_runner.h"
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
  base::TaskScheduler::Create("Updater");
  const auto task_scheduler_init_params =
      std::make_unique<base::TaskScheduler::InitParams>(
          base::SchedulerWorkerPoolParams(
              base::RecommendedMaxNumberOfThreadsInPool(3, 8, 0.1, 0),
              base::TimeDelta::FromSeconds(30)),
          base::SchedulerWorkerPoolParams(
              base::RecommendedMaxNumberOfThreadsInPool(3, 8, 0.1, 0),
              base::TimeDelta::FromSeconds(40)),
          base::SchedulerWorkerPoolParams(
              base::RecommendedMaxNumberOfThreadsInPool(8, 32, 0.3, 0),
              base::TimeDelta::FromSeconds(30)),
          base::SchedulerWorkerPoolParams(
              base::RecommendedMaxNumberOfThreadsInPool(8, 32, 0.3, 0),
              base::TimeDelta::FromSeconds(60)));
  base::TaskScheduler::GetInstance()->Start(*task_scheduler_init_params);
}

void TaskSchedulerStop() { base::TaskScheduler::GetInstance()->Shutdown(); }

std::unique_ptr<base::MessageLoopForUI> g_loop;
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

  g_loop.reset(new base::MessageLoopForUI());
  g_loop->Start();

  DCHECK(base::ThreadTaskRunnerHandle::IsSet());
  base::PlatformThread::SetName("UpdaterMain");

  network::NetworkModule::Options network_options;
  network_module.reset(new network::NetworkModule(network_options));

  updater_module.reset(new updater::UpdaterModule(network_module.get()));

  return 0;
}

}  // namespace updater
}  // namespace cobalt
