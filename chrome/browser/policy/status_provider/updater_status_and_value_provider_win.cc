// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/status_provider/updater_status_and_value_provider.h"

#include <windows.h>

#include <DSRole.h>

#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/install_static/install_util.h"

namespace {

std::string GetActiveDirectoryDomain() {
  std::string domain;
  ::DSROLE_PRIMARY_DOMAIN_INFO_BASIC* info = nullptr;
  if (::DsRoleGetPrimaryDomainInformation(nullptr,
                                          ::DsRolePrimaryDomainInfoBasic,
                                          (PBYTE*)&info) != ERROR_SUCCESS) {
    return domain;
  }
  if (info->DomainNameDns) {
    domain = base::WideToUTF8(info->DomainNameDns);
  }
  ::DsRoleFreeMemory(info);
  return domain;
}

}  // namespace

void UpdaterStatusAndValueProvider::Init() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&GetActiveDirectoryDomain),
      base::BindOnce(&UpdaterStatusAndValueProvider::OnDomainReceived,
                     weak_factory_.GetWeakPtr()));
}

void UpdaterStatusAndValueProvider::OnDomainReceived(std::string domain) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  domain_ = std::move(domain);
  // Call Refresh() to load the policies when the domain is received.
  Refresh();
}

// static
std::string UpdaterStatusAndValueProvider::GetUpdaterAppId() {
  return base::WideToUTF8(install_static::GetAppGuid());
}
