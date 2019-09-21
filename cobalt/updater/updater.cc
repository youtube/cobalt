// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "cobalt/updater/crash_client.h"
#include "cobalt/updater/crash_reporter.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "starboard/event.h"

namespace {

// For testing, CRX id is jebgalgnebhfojomionfpkfelancnnkf.
const char kTestId[] = "jebgalgnebhfojomionfpkfelancnnkf";
const uint8_t kTestIdHash[] = {0x94, 0x16, 0x0b, 0x6d, 0x41, 0x75, 0xe9, 0xec,
                               0x8e, 0xd5, 0xfa, 0x54, 0xb0, 0xd2, 0xdd, 0xa5,
                               0x6e, 0x05, 0x6b, 0xe8, 0x73, 0x47, 0xf6, 0xc4,
                               0x11, 0x9f, 0xbc, 0xb3, 0x09, 0xb3, 0x5b, 0x40};

// TODO: adjust config values if necessary.
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

void QuitLoop(base::OnceClosure quit_closure) { std::move(quit_closure).Run(); }

class Observer : public update_client::UpdateClient::Observer {
 public:
  explicit Observer(scoped_refptr<update_client::UpdateClient> update_client)
      : update_client_(update_client) {}

  // Overrides for update_client::UpdateClient::Observer.
  void OnEvent(Events event, const std::string& id) override {
    update_client_->GetCrxUpdateState(id, &crx_update_item_);
  }

  const update_client::CrxUpdateItem& crx_update_item() const {
    return crx_update_item_;
  }

 private:
  scoped_refptr<update_client::UpdateClient> update_client_;
  update_client::CrxUpdateItem crx_update_item_;
  DISALLOW_COPY_AND_ASSIGN(Observer);
};

void Return5() {}

// TODO: initiate an updater_module in browser/application.h, and bind these
// variables
std::unique_ptr<base::MessageLoopForUI> g_loop;
scoped_refptr<update_client::UpdateClient> uclient;
std::unique_ptr<Observer> observer;
std::unique_ptr<cobalt::network::NetworkModule> network_module;
std::unique_ptr<base::AtExitManager> exit_manager;

}  // namespace

namespace cobalt {
namespace updater {

int UpdaterMain(int argc, const char* const* argv) {
  exit_manager.reset(new base::AtExitManager());

  // TODO: Commandline args is only applicable to stand-alone updater.
  // Will remove it when updater is hooked with cobalt browser module.
  base::CommandLine::Init(argc, argv);

  // TODO: enable crash report with dependency on CrashPad

  // updater::crash_reporter::InitializeCrashKeys();

  // static crash_reporter::CrashKeyString<16> crash_key_process_type(
  //     "process_type");
  // crash_key_process_type.Set("updater");

  // if (CrashClient::GetInstance()->InitializeCrashReporting())
  //   VLOG(1) << "Crash reporting initialized.";
  // else
  //   VLOG(1) << "Crash reporting is not available.";

  // StartCrashReporter(UPDATER_VERSION_STRING);

  TaskSchedulerStart();

  g_loop.reset(new base::MessageLoopForUI());
  g_loop->Start();

  DCHECK(base::ThreadTaskRunnerHandle::IsSet());
  base::PlatformThread::SetName("UpdaterMain");

  network::NetworkModule::Options network_options;
  network_module.reset(new network::NetworkModule(network_options));
  auto config = base::MakeRefCounted<Configurator>(network_module.get());
  uclient = update_client::UpdateClientFactory(config);

  observer.reset(new Observer(uclient));
  uclient->AddObserver(observer.get());

  const std::vector<std::string> ids = {kTestId};

  base::Closure func_cb = base::Bind(&Return5);

  uclient->Update(
      ids,
      base::BindOnce(
          [](const std::vector<std::string>& ids)
              -> std::vector<base::Optional<update_client::CrxComponent>> {
            update_client::CrxComponent component;
            component.name = "jebg";
            component.pk_hash.assign(kTestIdHash,
                                     kTestIdHash + base::size(kTestIdHash));
            component.version = base::Version("0.0");
            return {component};
          }),
      true,
      base::BindOnce(
          [](base::OnceClosure closure, update_client::Error error) {
            base::ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, base::BindOnce(&QuitLoop, std::move(closure)));
          },
          func_cb));

  return 0;
}

}  // namespace updater
}  // namespace cobalt
