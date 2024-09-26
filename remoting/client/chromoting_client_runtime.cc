// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/chromoting_client_runtime.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "remoting/base/chromium_url_request.h"
#include "remoting/base/directory_service_client.h"
#include "remoting/base/mojo_util.h"
#include "remoting/base/oauth_token_getter_proxy.h"
#include "remoting/base/telemetry_log_writer.h"
#include "remoting/base/url_request_context_getter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/transitional_url_loader_factory_owner.h"

namespace remoting {

// static
ChromotingClientRuntime* ChromotingClientRuntime::GetInstance() {
  return base::Singleton<ChromotingClientRuntime>::get();
}

ChromotingClientRuntime::ChromotingClientRuntime() {
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("Remoting");

  DCHECK(!base::CurrentThread::Get());

  VLOG(1) << "Starting main message loop";
  ui_task_executor_ = std::make_unique<base::SingleThreadTaskExecutor>(
      base::MessagePumpType::UI);

  // |ui_task_executor_| runs on the main thread, so |ui_task_runner_| will run
  // on the main thread.  We can not kill the main thread when the message loop
  // becomes idle so the callback function does nothing (as opposed to the
  // typical base::MessageLoop::QuitClosure())
  ui_task_runner_ = new AutoThreadTaskRunner(ui_task_executor_->task_runner(),
                                             base::DoNothing());
  audio_task_runner_ = AutoThread::Create("native_audio", ui_task_runner_);
  display_task_runner_ = AutoThread::Create("native_disp", ui_task_runner_);
  network_task_runner_ = AutoThread::CreateWithType(
      "native_net", ui_task_runner_, base::MessagePumpType::IO);

  InitializeMojo();
}

ChromotingClientRuntime::~ChromotingClientRuntime() {
  if (delegate_) {
    delegate_->RuntimeWillShutdown();
  } else {
    DLOG(ERROR) << "ClientRuntime Delegate is null.";
  }

  // Block until tasks blocking shutdown have completed their execution.
  base::ThreadPoolInstance::Get()->Shutdown();

  if (delegate_) {
    delegate_->RuntimeDidShutdown();
  }
}

void ChromotingClientRuntime::Init(
    ChromotingClientRuntime::Delegate* delegate) {
  DCHECK(delegate);
  DCHECK(!delegate_);
  delegate_ = delegate;
  url_requester_ = new URLRequestContextGetter(network_task_runner_);
  log_writer_ = std::make_unique<TelemetryLogWriter>(CreateOAuthTokenGetter());
  network_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ChromotingClientRuntime::InitializeOnNetworkThread,
                     base::Unretained(this)));
}

std::unique_ptr<OAuthTokenGetter>
ChromotingClientRuntime::CreateOAuthTokenGetter() {
  return std::make_unique<OAuthTokenGetterProxy>(
      delegate_->oauth_token_getter(), ui_task_runner());
}

base::SequenceBound<DirectoryServiceClient>
ChromotingClientRuntime::CreateDirectoryServiceClient() {
  // A DirectoryServiceClient subclass that calls url_loader_factory() in its
  // constructor, as we can't call it on a non-network thread then pass it via
  // base::SequenceBound.
  class ClientDirectoryServiceClient : public DirectoryServiceClient {
   public:
    ClientDirectoryServiceClient(ChromotingClientRuntime* runtime,
                                 std::unique_ptr<OAuthTokenGetter> token_getter)
        : DirectoryServiceClient(token_getter.get(),
                                 runtime->url_loader_factory()),
          token_getter_(std::move(token_getter)) {}
    ~ClientDirectoryServiceClient() override = default;

   private:
    std::unique_ptr<OAuthTokenGetter> token_getter_;
  };

  return base::SequenceBound<ClientDirectoryServiceClient>(
      network_task_runner(), this, CreateOAuthTokenGetter());
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromotingClientRuntime::url_loader_factory() {
  DCHECK(network_task_runner()->BelongsToCurrentThread());
  return url_loader_factory_owner_->GetURLLoaderFactory();
}

void ChromotingClientRuntime::InitializeOnNetworkThread() {
  DCHECK(network_task_runner()->BelongsToCurrentThread());
  url_loader_factory_owner_ =
      std::make_unique<network::TransitionalURLLoaderFactoryOwner>(
          url_requester_);
  log_writer_->Init(url_loader_factory_owner_->GetURLLoaderFactory());
}

}  // namespace remoting
