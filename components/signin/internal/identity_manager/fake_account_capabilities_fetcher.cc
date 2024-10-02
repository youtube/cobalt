// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/fake_account_capabilities_fetcher.h"

#include "base/functional/callback.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

FakeAccountCapabilitiesFetcher::FakeAccountCapabilitiesFetcher(
    const CoreAccountInfo& account_info,
    OnCompleteCallback on_complete_callback,
    base::OnceClosure on_destroy_callback)
    : AccountCapabilitiesFetcher(account_info, std::move(on_complete_callback)),
      on_destroy_callback_(std::move(on_destroy_callback)) {}

FakeAccountCapabilitiesFetcher::~FakeAccountCapabilitiesFetcher() {
  std::move(on_destroy_callback_).Run();
}

void FakeAccountCapabilitiesFetcher::StartImpl() {}

void FakeAccountCapabilitiesFetcher::CompleteFetch(
    const absl::optional<AccountCapabilities>& account_capabilities) {
  CompleteFetchAndMaybeDestroySelf(account_capabilities);
}
