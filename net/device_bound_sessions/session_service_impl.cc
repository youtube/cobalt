// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_service_impl.h"

#include "base/functional/bind.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "net/base/schemeful_site.h"
#include "net/device_bound_sessions/session_store.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

namespace net::device_bound_sessions {

SessionServiceImpl::SessionServiceImpl(
    unexportable_keys::UnexportableKeyService& key_service,
    const URLRequestContext* request_context,
    SessionStore* store)
    : key_service_(key_service),
      context_(request_context),
      session_store_(store) {
  CHECK(context_);
}

SessionServiceImpl::~SessionServiceImpl() = default;

void SessionServiceImpl::LoadSessionsAsync() {
  if (!session_store_) {
    return;
  }
  session_store_->LoadSessions(base::BindOnce(
      &SessionServiceImpl::OnLoadSessionsComplete, weak_factory_.GetWeakPtr()));
}

void SessionServiceImpl::RegisterBoundSession(
    RegistrationFetcherParam registration_params,
    const IsolationInfo& isolation_info) {
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(registration_params), key_service_.get(), context_.get(),
      isolation_info,
      base::BindOnce(&SessionServiceImpl::OnRegistrationComplete,
                     weak_factory_.GetWeakPtr()));
}

void SessionServiceImpl::OnLoadSessionsComplete(
    SessionStore::SessionsMap sessions) {
  unpartitioned_sessions_.merge(sessions);
}

void SessionServiceImpl::OnRegistrationComplete(
    std::optional<RegistrationFetcher::RegistrationCompleteParams> params) {
  if (!params) {
    return;
  }

  auto session = Session::CreateIfValid(std::move(params->params), params->url);
  if (session) {
    session->set_unexportable_key_id(std::move(params->key_id));
    auto site = SchemefulSite(url::Origin::Create(params->url));
    if (session_store_) {
      session_store_->SaveSession(site, *session);
    }
    unpartitioned_sessions_.insert(std::make_pair(site, std::move(session)));
  }
  // Call Session::CreateIfValid(params). This callback will also need to take
  // the original request's info (in order to store the IsolationInfo etc).
  // Add the created session to the appropriate map, overwriting any existing
  // one that has the same SessionId.
}

std::pair<SessionServiceImpl::SessionsMap::iterator,
          SessionServiceImpl::SessionsMap::iterator>
SessionServiceImpl::GetSessionsForSite(const SchemefulSite& site) {
  auto now = base::Time::Now();
  auto [begin, end] = unpartitioned_sessions_.equal_range(site);
  for (auto it = begin; it != end;) {
    if (now >= it->second->expiry_date()) {
      if (session_store_) {
        session_store_->DeleteSession(site, it->second->id());
      }
      it = unpartitioned_sessions_.erase(it);
      // TODO(crbug.com/353774923): Clear BFCache entries for this session.
    } else {
      it->second->RecordAccess();
      it++;
    }
  }

  return unpartitioned_sessions_.equal_range(site);
}

std::optional<Session::Id> SessionServiceImpl::GetAnySessionRequiringDeferral(
    URLRequest* request) {
  SchemefulSite site(request->url());
  auto range = GetSessionsForSite(site);
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second->ShouldDeferRequest(request)) {
      return it->second->id();
    }
  }

  return std::nullopt;
}

// TODO(kristianm): Actually send the refresh request, for now continue
// with sending the deferred request right away.
void SessionServiceImpl::DeferRequestForRefresh(
    URLRequest* request,
    Session::Id session_id,
    RefreshCompleteCallback restart_callback,
    RefreshCompleteCallback continue_callback) {
  CHECK(restart_callback);
  CHECK(continue_callback);
  std::move(continue_callback).Run();
}

void SessionServiceImpl::SetChallengeForBoundSession(
    const GURL& request_url,
    const SessionChallengeParam& param) {
  if (!param.session_id()) {
    return;
  }

  SchemefulSite site(request_url);
  auto range = GetSessionsForSite(site);
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second->id().value() == param.session_id()) {
      it->second->set_cached_challenge(param.challenge());
      return;
    }
  }
}

Session* SessionServiceImpl::GetSessionForTesting(
    const SchemefulSite& site,
    const std::string& session_id) const {
  // Intentionally do not use `GetSessionsForSite` here so we do not
  // modify the session during testing.
  auto range = unpartitioned_sessions_.equal_range(site);
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second->id().value() == session_id) {
      return it->second.get();
    }
  }

  return nullptr;
}

}  // namespace net::device_bound_sessions
