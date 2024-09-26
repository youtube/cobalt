// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/facade/host_list_service.h"

#import <CoreFoundation/CoreFoundation.h>

#include <algorithm>

#import "remoting/ios/domain/user_info.h"
#import "remoting/ios/facade/remoting_authentication.h"
#import "remoting/ios/facade/remoting_service.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "remoting/base/directory_service_client.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/base/string_resources.h"
#include "remoting/base/task_util.h"
#include "remoting/client/chromoting_client_runtime.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util.h"

namespace remoting {

namespace {

HostListService::FetchFailureReason MapError(
    ProtobufHttpStatus::Code status_code) {
  switch (status_code) {
    case ProtobufHttpStatus::Code::UNAVAILABLE:
    case ProtobufHttpStatus::Code::DEADLINE_EXCEEDED:
      return HostListService::FetchFailureReason::NETWORK_ERROR;
    case ProtobufHttpStatus::Code::PERMISSION_DENIED:
    case ProtobufHttpStatus::Code::UNAUTHENTICATED:
      return HostListService::FetchFailureReason::AUTH_ERROR;
    default:
      return HostListService::FetchFailureReason::UNKNOWN_ERROR;
  }
}

// Returns true if |h1| should sort before |h2|.
bool CompareHost(const apis::v1::HostInfo& h1, const apis::v1::HostInfo& h2) {
  // Online hosts always sort before offline hosts.
  if (h1.status() != h2.status()) {
    return h1.status() == apis::v1::HostInfo_Status_ONLINE;
  }

  // Sort by host name.
  int name_compare = h1.host_name().compare(h2.host_name());
  if (name_compare != 0) {
    return name_compare < 0;
  }

  // Sort by last seen time if names are identical.
  return h1.last_seen_time() < h2.last_seen_time();
}

}  // namespace

HostListService* HostListService::GetInstance() {
  static base::NoDestructor<HostListService> instance;
  return instance.get();
}

HostListService::HostListService()
    : HostListService(ChromotingClientRuntime::GetInstance()
                          ->CreateDirectoryServiceClient()) {}

HostListService::HostListService(
    base::SequenceBound<DirectoryServiceClient> directory_client) {
  directory_client_ = std::move(directory_client);
  Init();
}

HostListService::~HostListService() {
  [NSNotificationCenter.defaultCenter removeObserver:user_update_observer_];
}

void HostListService::Init() {
  auto weak_this = weak_factory_.GetWeakPtr();
  user_update_observer_ = [NSNotificationCenter.defaultCenter
      addObserverForName:kUserDidUpdate
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification* notification) {
                UserInfo* user = notification.userInfo[kUserInfo];
                if (weak_this) {
                  weak_this->OnUserUpdated(user != nil);
                }
              }];
}

base::CallbackListSubscription HostListService::RegisterHostListStateCallback(
    const base::RepeatingClosure& callback) {
  return host_list_state_callbacks_.Add(callback);
}

base::CallbackListSubscription HostListService::RegisterFetchFailureCallback(
    const base::RepeatingClosure& callback) {
  return fetch_failure_callbacks_.Add(callback);
}

void HostListService::RequestFetch() {
  if (state_ == State::FETCHING) {
    return;
  }
  SetState(State::FETCHING);
  PostWithCallback(FROM_HERE, &directory_client_,
                   &DirectoryServiceClient::GetHostList,
                   base::BindOnce(&HostListService::HandleHostListResult,
                                  weak_factory_.GetWeakPtr()));
}

void HostListService::SetState(State state) {
  if (state == state_) {
    return;
  }
  if (state == State::NOT_FETCHED) {
    hosts_ = {};
  } else if (state == State::FETCHING || state == State::FETCHED) {
    last_fetch_failure_.reset();
  }
  state_ = state;
  host_list_state_callbacks_.Notify();
}

void HostListService::HandleHostListResult(
    const ProtobufHttpStatus& status,
    std::unique_ptr<apis::v1::GetHostListResponse> response) {
  if (!status.ok()) {
    HandleFetchFailure(status);
    return;
  }
  hosts_.clear();
  for (const auto& host : response->hosts()) {
    hosts_.push_back(host);
  }
  std::sort(hosts_.begin(), hosts_.end(), &CompareHost);
  SetState(State::FETCHED);
}

void HostListService::HandleFetchFailure(const ProtobufHttpStatus& status) {
  SetState(State::NOT_FETCHED);

  if (status.error_code() == ProtobufHttpStatus::Code::CANCELLED) {
    return;
  }

  last_fetch_failure_ = std::make_unique<FetchFailureInfo>();
  last_fetch_failure_->reason = MapError(status.error_code());

  switch (last_fetch_failure_->reason) {
    case FetchFailureReason::NETWORK_ERROR:
      last_fetch_failure_->localized_description =
          l10n_util::GetStringUTF8(IDS_ERROR_NETWORK_ERROR);
      break;
    case FetchFailureReason::AUTH_ERROR:
      last_fetch_failure_->localized_description =
          l10n_util::GetStringUTF8(IDS_ERROR_OAUTH_TOKEN_INVALID);
      break;
    default:
      last_fetch_failure_->localized_description = status.error_message();
  }
  LOG(WARNING) << "Failed to fetch host list: "
               << last_fetch_failure_->localized_description
               << " reason: " << static_cast<int>(last_fetch_failure_->reason);
  fetch_failure_callbacks_.Notify();
  if (last_fetch_failure_->reason == FetchFailureReason::AUTH_ERROR) {
    [RemotingService.instance.authentication logout];
  }
}

void HostListService::OnUserUpdated(bool is_user_signed_in) {
  directory_client_.AsyncCall(&DirectoryServiceClient::CancelPendingRequests);
  SetState(State::NOT_FETCHED);
  if (is_user_signed_in) {
    RequestFetch();
  }
}

}  // namespace remoting
