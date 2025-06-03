// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/init/web_main_loop.h"

#import <stddef.h>

#import <utility>

#import "base/command_line.h"
#import "base/functional/bind.h"
#import "base/logging.h"
#import "base/message_loop/message_pump_type.h"
#import "base/metrics/histogram_macros.h"
#import "base/path_service.h"
#import "base/power_monitor/power_monitor.h"
#import "base/power_monitor/power_monitor_device_source.h"
#import "base/process/process_metrics.h"
#import "base/task/single_thread_task_executor.h"
#import "base/task/single_thread_task_runner.h"
#import "base/task/thread_pool/thread_pool_instance.h"
#import "base/threading/thread_restrictions.h"
#import "ios/web/net/cookie_notification_bridge.h"
#import "ios/web/public/init/ios_global_state.h"
#import "ios/web/public/init/web_main_parts.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_client.h"
#import "ios/web/web_sub_thread.h"
#import "ios/web/web_thread_impl.h"
#import "ios/web/webui/url_data_manager_ios.h"

namespace web {

// The currently-running WebMainLoop.  There can be one or zero.
// TODO(crbug.com/965889): Desktop uses this to implement
// ImmediateShutdownAndExitProcess.  If we don't need that functionality, we can
// remove this.
WebMainLoop* g_current_web_main_loop = nullptr;

WebMainLoop::WebMainLoop()
    : result_code_(0),
      created_threads_(false),
      destroy_task_executor_(
          base::BindOnce(&ios_global_state::DestroySingleThreadTaskExecutor)),
      destroy_network_change_notifier_(
          base::BindOnce(&ios_global_state::DestroyNetworkChangeNotifier)) {
  DCHECK(!g_current_web_main_loop);
  g_current_web_main_loop = this;
}

WebMainLoop::~WebMainLoop() {
  DCHECK_EQ(this, g_current_web_main_loop);
  g_current_web_main_loop = nullptr;
}

void WebMainLoop::Init() {
  parts_ = web::GetWebClient()->CreateWebMainParts();
}

void WebMainLoop::EarlyInitialization() {
  if (parts_) {
    parts_->PreEarlyInitialization();
    parts_->PostEarlyInitialization();
  }
}

void WebMainLoop::CreateMainMessageLoop() {
  if (parts_) {
    parts_->PreCreateMainMessageLoop();
  }

  ios_global_state::BuildSingleThreadTaskExecutor();

  InitializeMainThread();

  ios_global_state::CreateNetworkChangeNotifier();

  if (parts_) {
    parts_->PostCreateMainMessageLoop();
  }
}

void WebMainLoop::CreateStartupTasks() {
  int result = 0;
  result = PreCreateThreads();
  if (result > 0)
    return;

  result = CreateThreads();
  if (result > 0)
    return;

  result = PostCreateThreads();
  if (result > 0) {
    return;
  }

  result = WebThreadsStarted();
  if (result > 0)
    return;

  result = PreMainMessageLoopRun();
  if (result > 0)
    return;
}

int WebMainLoop::PreCreateThreads() {
  if (parts_) {
    parts_->PreCreateThreads();
  }

  // TODO(crbug.com/807279): Do we need PowerMonitor on iOS, or can we get rid
  // of it?
  // TODO(crbug.com/1370276): Remove this once we have confidence PowerMonitor
  // is not needed for iOS
  base::PowerMonitor::Initialize(
      std::make_unique<base::PowerMonitorDeviceSource>());

  return result_code_;
}

int WebMainLoop::PostCreateThreads() {
  if (parts_) {
    parts_->PostCreateThreads();
  }
  return result_code_;
}

int WebMainLoop::CreateThreads() {
  ios_global_state::StartThreadPool();

  base::Thread::Options io_message_loop_options;
  io_message_loop_options.message_pump_type = base::MessagePumpType::IO;
  io_thread_ = std::make_unique<WebSubThread>(WebThread::IO);
  if (!io_thread_->StartWithOptions(std::move(io_message_loop_options)))
    LOG(FATAL) << "Failed to start WebThread::IO";
  io_thread_->RegisterAsWebThread();

  // Only start IO thread above as this is the only WebThread besides UI (which
  // is the main thread).
  static_assert(WebThread::ID_COUNT == 2, "Unhandled WebThread");

  created_threads_ = true;
  return result_code_;
}

int WebMainLoop::PreMainMessageLoopRun() {
  if (parts_) {
    parts_->PreMainMessageLoopRun();
  }

  // If the UI thread blocks, the whole UI is unresponsive.
  // Do not allow unresponsive tasks from the UI thread.
  base::DisallowUnresponsiveTasks();
  return result_code_;
}

void WebMainLoop::ShutdownThreadsAndCleanUp() {
  if (!created_threads_) {
    // Called early, nothing to do
    return;
  }

  // Teardown may start in PostMainMessageLoopRun, and during teardown we
  // need to be able to perform IO.
  base::PermanentThreadAllowance::AllowBlocking();
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(base::IgnoreResult(
                     &base::PermanentThreadAllowance::AllowBlocking)));

  // Also allow waiting to join threads.
  // TODO(crbug.com/800808): Ideally this (and the above AllowBlocking() would
  // be scoped allowances). That would be one of the first step to ensure no
  // persistent work is being done after ThreadPoolInstance::Shutdown() in order
  // to move towards atomic shutdown.
  base::PermanentThreadAllowance::AllowBaseSyncPrimitives();
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(
          &base::PermanentThreadAllowance::AllowBaseSyncPrimitives)));

  if (parts_) {
    parts_->PostMainMessageLoopRun();
  }

  io_thread_.reset();

  // Only stop IO thread above as this is the only WebThread besides UI (which
  // is the main thread).
  static_assert(WebThread::ID_COUNT == 2, "Unhandled WebThread");

  // Shutdown ThreadPool after the other threads. Other threads such as the
  // I/O thread may need to schedule work like closing files or flushing data
  // during shutdown, so ThreadPool needs to be available. There may also be
  // slow operations pending that will block shutdown, so closing it here (which
  // will block until required operations are complete) gives more head start
  // for those operations to finish.
  base::ThreadPoolInstance::Get()->Shutdown();

  URLDataManagerIOS::DeleteDataSources();

  if (parts_) {
    parts_->PostDestroyThreads();
  }
}

void WebMainLoop::InitializeMainThread() {
  base::PlatformThread::SetName("CrWebMain");

  // Register the main thread by instantiating it, but don't call any methods.
  DCHECK(base::SingleThreadTaskRunner::HasCurrentDefault());
  main_thread_.reset(new WebThreadImpl(
      WebThread::UI,
      ios_global_state::GetMainThreadTaskExecutor()->task_runner()));
}

int WebMainLoop::WebThreadsStarted() {
  cookie_notification_bridge_.reset(new CookieNotificationBridge);
  return result_code_;
}

}  // namespace web
