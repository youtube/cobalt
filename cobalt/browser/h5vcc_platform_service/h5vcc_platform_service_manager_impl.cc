// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/h5vcc_platform_service/h5vcc_platform_service_manager_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "cobalt/browser/features.h"
#include "cobalt/browser/h5vcc_platform_service/platform_service_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "starboard/extension/platform_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/java_handler_thread.h"
#endif

namespace h5vcc_platform_service {

namespace {

#if !BUILDFLAG(IS_ANDROID)
base::Lock& GetTaskRunnerLock() {
  static base::NoDestructor<base::Lock> s_lock;
  return *s_lock;
}

scoped_refptr<base::SingleThreadTaskRunner>& GetTaskRunnerStorage() {
  static base::NoDestructor<scoped_refptr<base::SingleThreadTaskRunner>>
      s_task_runner;
  return *s_task_runner;
}
#endif

bool HasPlatformService(const std::string& service_name) {
  const base::ElapsedTimer timer;
  const CobaltExtensionPlatformServiceApi* platform_service_extension =
      PlatformServiceImpl::GetPlatformServiceApi();
  if (!platform_service_extension) {
    LOG(WARNING) << "The platform service extension is not implemented on this "
                 << "platform";
    base::UmaHistogramMicrosecondsTimes("Cobalt.PlatformService.Has.DurationUs",
                                        timer.Elapsed());
    return false;
  }
  const bool exists = platform_service_extension->Has(service_name.c_str());
  base::UmaHistogramMicrosecondsTimes("Cobalt.PlatformService.Has.DurationUs",
                                      timer.Elapsed());
  return exists;
}

}  // namespace

// Manages H5vccPlatformServiceManager and PlatformService Mojo receivers off
// the browser UI thread for a single document when
// features::kCobaltPlatformServicesOffUiThread is enabled.
//
// Lifetime: Owned by H5vccPlatformServiceManagerImpl (a DocumentUserData) via
// base::SequenceBound; destroyed on GetPlatformServiceTaskRunner() when the
// owning document is destroyed.
// Threading: Affine to the dedicated platform service sequence returned by
// GetPlatformServiceTaskRunner().
class H5vccPlatformServiceManagerImpl::OffUiThreadPlatformServiceManager
    : public mojom::H5vccPlatformServiceManager {
 public:
  OffUiThreadPlatformServiceManager() = default;
  ~OffUiThreadPlatformServiceManager() override = default;

  void Bind(
      mojo::PendingReceiver<mojom::H5vccPlatformServiceManager> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  void Has(const std::string& service_name, HasCallback callback) override {
    std::move(callback).Run(HasPlatformService(service_name));
  }

  void Open(const std::string& service_name,
            mojo::PendingRemote<mojom::PlatformServiceObserver> observer,
            mojo::PendingReceiver<mojom::PlatformService> receiver,
            OpenCallback callback) override {
    auto instance = PlatformServiceImpl::CreateOffUiThread(service_name,
                                                           std::move(observer));
    if (instance) {
      service_receivers_.Add(std::move(instance), std::move(receiver));
    }
    std::move(callback).Run();
  }

 private:
  mojo::ReceiverSet<mojom::H5vccPlatformServiceManager> receivers_;
  mojo::UniqueReceiverSet<mojom::PlatformService> service_receivers_;
};

scoped_refptr<base::SingleThreadTaskRunner> GetPlatformServiceTaskRunner() {
#if BUILDFLAG(IS_ANDROID)
  static base::NoDestructor<std::unique_ptr<base::android::JavaHandlerThread>>
      s_thread([] {
        auto thread = std::make_unique<base::android::JavaHandlerThread>(
            "CobaltPlatformService");
        thread->Start();
        return thread;
      }());
  return (*s_thread)->task_runner();
#else
  base::AutoLock auto_lock(GetTaskRunnerLock());
  auto& runner = GetTaskRunnerStorage();
  if (!runner) {
    runner = base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  }
  return runner;
#endif
}

void ResetPlatformServiceTaskRunnerForTesting() {
#if !BUILDFLAG(IS_ANDROID)
  base::AutoLock auto_lock(GetTaskRunnerLock());
  GetTaskRunnerStorage().reset();
#endif
}

DOCUMENT_USER_DATA_KEY_IMPL(H5vccPlatformServiceManagerImpl);

// static
void H5vccPlatformServiceManagerImpl::GetOrCreate(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::H5vccPlatformServiceManager> receiver) {
  if (!render_frame_host || !render_frame_host->IsActive()) {
    return;
  }

  H5vccPlatformServiceManagerImpl* instance =
      H5vccPlatformServiceManagerImpl::GetOrCreateForCurrentDocument(
          render_frame_host);
  if (instance) {
    instance->BindReceiver(std::move(receiver));
  } else {
    LOG(ERROR) << "Failed to GetOrCreate H5vccPlatformServiceManagerImpl";
  }
}

void H5vccPlatformServiceManagerImpl::BindReceiver(
    mojo::PendingReceiver<mojom::H5vccPlatformServiceManager> receiver) {
  if (base::FeatureList::IsEnabled(
          cobalt::features::kCobaltPlatformServicesOffUiThread)) {
    if (!off_ui_thread_manager_) {
      off_ui_thread_manager_.emplace(GetPlatformServiceTaskRunner());
    }
    off_ui_thread_manager_.AsyncCall(&OffUiThreadPlatformServiceManager::Bind)
        .WithArgs(std::move(receiver));
    return;
  }
  receivers_.Add(this, std::move(receiver));
}

H5vccPlatformServiceManagerImpl::H5vccPlatformServiceManagerImpl(
    content::RenderFrameHost* render_frame_host)
    : content::DocumentUserData<H5vccPlatformServiceManagerImpl>(
          render_frame_host) {
  DLOG(INFO) << "H5vccPlatformServiceManagerImpl created for RFH ID: "
             << this->render_frame_host().GetGlobalId();

  // A disconnect handler is set in order to provide debug logging but is not
  // functionally required: resource management is provided by mojo::ReceiverSet
  // and content::DocumentUserData.
  //
  // mojo::ReceiverSet automatically cleans up receivers that have been
  // disconnected.
  //
  // DocumentUserData handles object destruction when the RFH is destroyed.
  // If receivers_.empty(), this instance remains until the RFH is destroyed.
  receivers_.set_disconnect_handler(base::BindRepeating(
      &H5vccPlatformServiceManagerImpl::OnReceiverDisconnect,
      weak_factory_.GetWeakPtr()));
}

H5vccPlatformServiceManagerImpl::~H5vccPlatformServiceManagerImpl() {
  DLOG(INFO) << "H5vccPlatformServiceManagerImpl destroyed for RFH ID: "
             << this->render_frame_host().GetGlobalId();
}

void H5vccPlatformServiceManagerImpl::OnReceiverDisconnect() {
  DLOG(INFO) << "A H5vccPlatformServiceManagerImpl receiver disconnected for "
             << "RFH ID: " << render_frame_host().GetGlobalId();
}

void H5vccPlatformServiceManagerImpl::Has(const std::string& service_name,
                                          HasCallback callback) {
  std::move(callback).Run(HasPlatformService(service_name));
}

void H5vccPlatformServiceManagerImpl::Open(
    const std::string& service_name,
    mojo::PendingRemote<mojom::PlatformServiceObserver> observer,
    mojo::PendingReceiver<mojom::PlatformService> receiver,
    OpenCallback callback) {
  if (!render_frame_host().IsActive()) {
    LOG(ERROR)
        << "H5vccPlatformServiceManagerImpl::Open: RenderFrameHost not active";
    // Do not run the callback, signaling an error to the [Sync] caller.
    return;
  }

  // Pass the RenderFrameHost& from the base class to
  // PlatformServiceImpl::Create.
  PlatformServiceImpl::Create(render_frame_host(), service_name,
                              std::move(observer), std::move(receiver));

  // Running the callback unblocks the [Sync] call in the renderer.
  std::move(callback).Run();
}

}  // namespace h5vcc_platform_service
