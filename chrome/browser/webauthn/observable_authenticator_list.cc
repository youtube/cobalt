// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/observable_authenticator_list.h"

#include <utility>

#include "base/ranges/algorithm.h"
#include "chrome/browser/webauthn/authenticator_list_observer.h"

ObservableAuthenticatorList::ObservableAuthenticatorList() = default;

ObservableAuthenticatorList::ObservableAuthenticatorList(
    ObservableAuthenticatorList&&) = default;

ObservableAuthenticatorList& ObservableAuthenticatorList::operator=(
    ObservableAuthenticatorList&&) = default;

ObservableAuthenticatorList::~ObservableAuthenticatorList() = default;

void ObservableAuthenticatorList::AddAuthenticator(
    AuthenticatorReference authenticator) {
  authenticator_list_.emplace_back(std::move(authenticator));
  if (observer_)
    observer_->OnAuthenticatorAdded(authenticator_list_.back());
}

void ObservableAuthenticatorList::RemoveAuthenticator(
    base::StringPiece authenticator_id) {
  auto it = GetAuthenticatorIterator(authenticator_id);
  if (it == authenticator_list_.end())
    return;

  auto removed_authenticator = std::move(*it);
  authenticator_list_.erase(it);

  if (observer_)
    observer_->OnAuthenticatorRemoved(removed_authenticator);
}

void ObservableAuthenticatorList::RemoveAllAuthenticators() {
  if (observer_) {
    for (const auto& authenticator : authenticator_list_)
      observer_->OnAuthenticatorRemoved(authenticator);
  }
  authenticator_list_.clear();
}

AuthenticatorReference* ObservableAuthenticatorList::GetAuthenticator(
    base::StringPiece authenticator_id) {
  auto it = GetAuthenticatorIterator(authenticator_id);
  if (it == authenticator_list_.end())
    return nullptr;

  return &*it;
}

void ObservableAuthenticatorList::SetObserver(
    AuthenticatorListObserver* observer) {
  DCHECK(!observer_);
  observer_ = observer;
}

void ObservableAuthenticatorList::RemoveObserver() {
  observer_ = nullptr;
}

ObservableAuthenticatorList::AuthenticatorListIterator
ObservableAuthenticatorList::GetAuthenticatorIterator(
    base::StringPiece authenticator_id) {
  return base::ranges::find(authenticator_list_, authenticator_id,
                            &AuthenticatorReference::authenticator_id);
}
