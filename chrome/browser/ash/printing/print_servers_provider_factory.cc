// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/print_servers_provider_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/printing/print_servers_provider.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "content/public/browser/browser_thread.h"

namespace ash {

// static
PrintServersProviderFactory* PrintServersProviderFactory::Get() {
  static base::NoDestructor<PrintServersProviderFactory> instance;
  return instance.get();
}

base::WeakPtr<PrintServersProvider>
PrintServersProviderFactory::GetForAccountId(const AccountId& account_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto found = providers_by_user_.find(account_id);
  if (found != providers_by_user_.end()) {
    return found->second->AsWeakPtr();
  }

  providers_by_user_[account_id] = PrintServersProvider::Create();
  return providers_by_user_[account_id]->AsWeakPtr();
}

base::WeakPtr<PrintServersProvider> PrintServersProviderFactory::GetForProfile(
    Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return nullptr;

  return GetForAccountId(user->GetAccountId());
}

base::WeakPtr<PrintServersProvider>
PrintServersProviderFactory::GetForDevice() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!device_provider_) {
    device_provider_ = PrintServersProvider::Create();
  }
  return device_provider_->AsWeakPtr();
}

void PrintServersProviderFactory::RemoveForAccountId(
    const AccountId& account_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  providers_by_user_.erase(account_id);
}

void PrintServersProviderFactory::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  providers_by_user_.clear();
  device_provider_ = nullptr;
}

PrintServersProviderFactory::PrintServersProviderFactory() = default;
PrintServersProviderFactory::~PrintServersProviderFactory() = default;

}  // namespace ash
