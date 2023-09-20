// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/ping_manager.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/update_client/component.h"
#include "components/update_client/configurator.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/protocol_serializer.h"
#include "components/update_client/request_sender.h"
#include "components/update_client/utils.h"
#include "url/gurl.h"

namespace update_client {

namespace {

const int kErrorNoEvents = -1;
const int kErrorNoUrl = -2;

// When building for STARBOARD add the PingSender to the update_client namespace
// as we keep a reference to it in PingManager.
#if defined(STARBOARD)
}
#endif

// An instance of this class can send only one ping.
class PingSender : public base::RefCountedThreadSafe<PingSender> {
 public:
  using Callback = PingManager::Callback;
  explicit PingSender(scoped_refptr<Configurator> config);
  void SendPing(const Component& component, Callback callback);

#if defined(STARBOARD)
  void Cancel();
#endif

 protected:
  virtual ~PingSender();

 private:
  friend class base::RefCountedThreadSafe<PingSender>;
  void SendPingComplete(int error,
                        const std::string& response,
                        int retry_after_sec);

  THREAD_CHECKER(thread_checker_);

  const scoped_refptr<Configurator> config_;
  Callback callback_;
  std::unique_ptr<RequestSender> request_sender_;

  DISALLOW_COPY_AND_ASSIGN(PingSender);
};

PingSender::PingSender(scoped_refptr<Configurator> config) : config_(config) {}

PingSender::~PingSender() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void PingSender::SendPing(const Component& component, Callback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

#if defined(STARBOARD)
  LOG(INFO) << "PingSender::SendPing";
#endif

  if (component.events().empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), kErrorNoEvents, ""));
    return;
  }

  DCHECK(component.crx_component());

  auto urls(config_->PingUrl());
  if (component.crx_component()->requires_network_encryption)
    RemoveUnsecureUrls(&urls);

  if (urls.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), kErrorNoUrl, ""));
    return;
  }

  callback_ = std::move(callback);

  std::vector<protocol_request::App> apps;
  apps.push_back(MakeProtocolApp(component.id(),
                                 component.crx_component()->version,
                                 component.GetEvents()));
  request_sender_ = std::make_unique<RequestSender>(config_);
  request_sender_->Send(
      urls, {},
      config_->GetProtocolHandlerFactory()->CreateSerializer()->Serialize(
          MakeProtocolRequest(
              component.session_id(), config_->GetProdId(),
              config_->GetBrowserVersion().GetString(), config_->GetLang(),
              config_->GetChannel(), config_->GetOSLongName(),
              config_->GetDownloadPreference(), config_->ExtraRequestParams(),
              nullptr, std::move(apps))),
      false, base::BindOnce(&PingSender::SendPingComplete, this));
}

#if defined(STARBOARD)
void PingSender::Cancel() {
  LOG(INFO) << "PingSender::Cancel";
  if (request_sender_.get()) {
    request_sender_->Cancel();
  }
}
#endif

void PingSender::SendPingComplete(int error,
                                  const std::string& response,
                                  int retry_after_sec) {
  LOG(INFO) << "PingSender::SendPingComplete";
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(callback_).Run(error, response);
}

#if !defined(STARBOARD)
}  // namespace
#endif

PingManager::PingManager(scoped_refptr<Configurator> config)
    : config_(config) {
#if defined(STARBOARD)
  LOG(INFO) << "PingManager::PingManager";
#endif
}

#if defined(STARBOARD)
void PingManager::Cancel() {
  LOG(INFO) << "PingManager::Cancel";
  if (ping_sender_.get()) {
    ping_sender_->Cancel();
  }
}
#endif

PingManager::~PingManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if defined(STARBOARD)
  LOG(INFO) << "PingManager::~PingManager";
#endif
}

void PingManager::SendPing(const Component& component, Callback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

#if defined(STARBOARD)
  LOG(INFO) << "PingManager::SendPing";

  ping_sender_ = base::MakeRefCounted<PingSender>(config_);
  ping_sender_->SendPing(component, std::move(callback));
#else
  auto ping_sender = base::MakeRefCounted<PingSender>(config_);
  ping_sender->SendPing(component, std::move(callback));
#endif
}

}  // namespace update_client
