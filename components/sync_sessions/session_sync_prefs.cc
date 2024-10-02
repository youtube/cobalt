// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/session_sync_prefs.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace sync_sessions {
namespace {

// Legacy GUID to identify this client, no longer newly populated by modern
// clients but honored if present.
const char kLegacySyncSessionsGUID[] = "sync.session_sync_guid";

}  // namespace

// static
void SessionSyncPrefs::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kLegacySyncSessionsGUID, std::string());
}

SessionSyncPrefs::SessionSyncPrefs(PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service);
}

SessionSyncPrefs::~SessionSyncPrefs() = default;

std::string SessionSyncPrefs::GetLegacySyncSessionsGUID() const {
  return pref_service_->GetString(kLegacySyncSessionsGUID);
}

void SessionSyncPrefs::ClearLegacySyncSessionsGUID() {
  pref_service_->ClearPref(kLegacySyncSessionsGUID);
}

void SessionSyncPrefs::SetLegacySyncSessionsGUIDForTesting(
    const std::string& guid) {
  pref_service_->SetString(kLegacySyncSessionsGUID, guid);
}

}  // namespace sync_sessions
