// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_verification_tokens/private_verification_tokens_service.h"

#include <string>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/private_verification_tokens/common/private_verification_tokens_store.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

const base::FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("PrivateVerificationTokens");

using private_verification_tokens::PrivateVerificationTokensStore;

}  // namespace

// static
std::unique_ptr<PrivateVerificationTokensService>
PrivateVerificationTokensService::Create(const base::FilePath& data_directory) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (data_directory.empty()) {
    return nullptr;
  }
  auto service = base::WrapUnique(new PrivateVerificationTokensService());
  base::FilePath db_path = data_directory.Append(kDatabaseName);

  auto store = PrivateVerificationTokensStore::Create(
      db_path,
      base::BindOnce(&PrivateVerificationTokensService::OnStoreInitialized,
                     service->weak_ptr_factory_.GetWeakPtr()));

  if (!store) {
    return nullptr;
  }

  service->store_ = std::move(store);
  return service;
}

PrivateVerificationTokensService::PrivateVerificationTokensService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

PrivateVerificationTokensService::~PrivateVerificationTokensService() = default;

void PrivateVerificationTokensService::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_shutting_down_) {
    return;
  }
  is_shutting_down_ = true;
  store_ = nullptr;
  receivers_.Clear();
}

void PrivateVerificationTokensService::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observers_.AddObserver(observer);
}

void PrivateVerificationTokensService::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observers_.RemoveObserver(observer);
}

bool PrivateVerificationTokensService::is_initialized() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return store_ && store_->is_initialized();
}

void PrivateVerificationTokensService::BindReceiver(
    mojo::PendingReceiver<
        private_verification_tokens::mojom::PrivateVerificationTokensProvider>
        pending_receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_shutting_down_) {
    return;
  }
  receivers_.Add(this, std::move(pending_receiver));
}

void PrivateVerificationTokensService::GetTokens(
    private_verification_tokens::mojom::PrivateVerificationTokensProvider::
        GetTokensCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_shutting_down_) {
    std::move(callback).Run({});
    return;
  }
  CHECK(store_);
  std::vector<
      private_verification_tokens::mojom::PrivateVerificationTokensTokenPtr>
      tokens;
  for (const auto& [issuer, token_with_id] : store_->tokens()) {
    auto mojo_token = private_verification_tokens::mojom::
        PrivateVerificationTokensToken::New();
    mojo_token->issuer = issuer;
    mojo_token->serialized_token = token_with_id.token.token();
    tokens.push_back(std::move(mojo_token));
  }
  std::move(callback).Run(std::move(tokens));
}

void PrivateVerificationTokensService::OnStoreInitialized() {
  if (is_shutting_down_) {
    return;
  }
  for (auto& observer : observers_) {
    observer.OnInitializationComplete();
  }
}
