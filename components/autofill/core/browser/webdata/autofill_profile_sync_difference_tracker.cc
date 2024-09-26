// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_profile_sync_difference_tracker.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile_sync_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/webdata/autofill_profile_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/sync/model/model_error.h"

namespace autofill {

using absl::optional;
using syncer::ModelError;

// Simplify checking for optional errors and returning only when present.
#define RETURN_IF_ERROR(x)              \
  if (optional<ModelError> error = x) { \
    return error;                       \
  }

AutofillProfileSyncDifferenceTracker::AutofillProfileSyncDifferenceTracker(
    AutofillTable* table)
    : table_(table) {}

AutofillProfileSyncDifferenceTracker::~AutofillProfileSyncDifferenceTracker() {}

optional<ModelError>
AutofillProfileSyncDifferenceTracker::IncorporateRemoteProfile(
    std::unique_ptr<AutofillProfile> remote) {
  const std::string remote_storage_key =
      GetStorageKeyFromAutofillProfile(*remote);

  if (!GetLocalOnlyEntries()) {
    return ModelError(FROM_HERE, "Failed reading from WebDatabase.");
  }

  optional<AutofillProfile> local_with_same_storage_key =
      ReadEntry(remote_storage_key);

  if (local_with_same_storage_key) {
    // The remote profile already exists locally with the same key. Update
    // the local entry with remote data.
    std::unique_ptr<AutofillProfile> updated =
        std::make_unique<AutofillProfile>(local_with_same_storage_key.value());
    // We ignore remote updates to a verified profile because we want to keep
    // the exact version that the user edited by hand.
    if (local_with_same_storage_key->IsVerified() && !remote->IsVerified()) {
      return absl::nullopt;
    }
    updated->OverwriteDataFrom(*remote);
    // TODO(crbug.com/1117022l): if |updated| deviates from |remote|, we should
    // sync it back up. The only way |updated| can differ is having some extra
    // fields compared to |remote|. Thus, this cannot lead to an infinite loop
    // of commits from two clients as each commit decreases the set of empty
    // fields. This invariant depends on the implementation of
    // OverwriteDataFrom() and thus should be enforced by a DCHECK.
    //
    // With structured names the situation changes a bit,
    // but maintains its character.
    // If the name stored in |remote| is mergeable with the local |name|, the
    // merge operation is performed. Otherwise the name structure of |local| is
    // maintained iff |remote| contains an empty name.
    // A merge operations manipulates the name towards a better total
    // verification status of the stored tokens. Consequently, it is not
    // possible to get into a merging loop of two competing clients.
    // As in the legacy implementation, |OverwriteDataForm()| can change the
    // profile in a deterministic way and a direct sync-back would be
    // reasonable.

    if (!updated->EqualsForSyncPurposes(*local_with_same_storage_key)) {
      // We need to write back locally new changes in this entry.
      update_to_local_.push_back(std::move(updated));
    }
    GetLocalOnlyEntries()->erase(remote_storage_key);
    return absl::nullopt;
  }

  // Check if profile appears under a different storage key to be de-duplicated.
  // TODO(crbug.com/1043683): Deal with rare cases when an remote update
  // contains several exact duplicates (with different guids). We should not
  // only search in local only entries but also in |update_to_local_| and
  // |add_to_local_|. Likely needs a bit of refactoring to make the resulting
  // code easy to understand.
  for (const auto& [local_storage_key, local] : *GetLocalOnlyEntries()) {
    // Look for exact duplicates, compare only profile contents (and
    // ignore origin and language code in comparison).
    if (local->Compare(*remote) == 0) {
      // A duplicate found: keep the version with the bigger storage key.
      DVLOG(2)
          << "[AUTOFILL SYNC] The profile "
          << base::UTF16ToUTF8(local->GetRawInfo(NAME_FIRST))
          << base::UTF16ToUTF8(local->GetRawInfo(NAME_LAST))
          << " already exists with a different storage key*; keep the bigger "
          << (remote_storage_key > local_storage_key ? "remote" : "local")
          << " key " << std::max(remote_storage_key, local_storage_key)
          << " and delete the smaller key "
          << std::min(remote_storage_key, local_storage_key);
      if (remote_storage_key > local_storage_key) {
        // We keep the remote entity and delete the local one.
        // Ensure that a verified profile can never revert back to an unverified
        // one. In such a case, take over the old origin for the new entry.
        if (local->IsVerified() && !remote->IsVerified()) {
          remote->set_origin(local->origin());
          // Save a copy of the remote profile also* to sync.
          save_to_sync_.push_back(std::make_unique<AutofillProfile>(*remote));
        }
        add_to_local_.push_back(std::move(remote));
        // Deleting from sync is a no-op if it is local-only so far.
        // There are a few ways how a synced local entry A could theoretically
        // receive a remote duplicate B with a larger GUID:
        //  1) Remote entity B got uploaded by another client through initial
        //     sync. That client thus also knew about A and issued a deletion of
        //     A at the same time. This client (if receiving creation of B
        //     first) resolves the conflict in the same way and re-issues the
        //     deletion of A. In most cases the redundant deletion does not even
        //     get sent as the processor already knows A got deleted remotely.
        //  2) Remote entity B got uploaded by another client through race
        //     condition (i.e. not knowing about A, yet). In practice, this only
        //     happens when two clients simultaneously convert a server profile
        //     into local profiles. If the other client goes offline before
        //     receiving A, this client is responsible for deleting A from the
        //     server and thus must issue a deletion. (In most cases, the other
        //     client does not go offline and thus both clients issue a deletion
        //     of A independently).
        //  3) (a paranoid case) Remote entity B got uploaded by another client
        //     by an error, i.e. already as a duplicate given their local state.
        //     Through standard flows, it should be impossible (duplicates are
        //     cought early in PDM code so such a change attempt does not even
        //     propagate to the sync bridge). Still, it's good to treat this
        //     case here for robustness.
        delete_from_sync_.insert(local_storage_key);
        RETURN_IF_ERROR(DeleteFromLocal(local_storage_key));
      } else {
        // We keep the local entity and delete the remote one.
        // Ensure that a verified profile can never revert back to an unverified
        // one. In such a case, modify the origin and re-upload. Otherwise,
        // there's no need to upload it: either is was already uploaded before
        // (if this is incremental sync) or we'll upload it with all the
        // remaining data in GetLocalOnlyEntries (if this is an initial sync).
        if (remote->IsVerified() && !local->IsVerified()) {
          auto modified_local = std::make_unique<AutofillProfile>(*local);
          modified_local->set_origin(remote->origin());
          update_to_local_.push_back(
              std::make_unique<AutofillProfile>(*modified_local));
          save_to_sync_.push_back(std::move(modified_local));
          // The local entity is already marked for upload so it is not local
          // only anymore (we do not want to upload it once again while flushing
          // if this is initial sync).
          GetLocalOnlyEntries()->erase(local_storage_key);
        }
        delete_from_sync_.insert(remote_storage_key);
      }
      return absl::nullopt;
    }
  }

  // If no duplicate was found, just add the remote profile.
  add_to_local_.push_back(std::move(remote));
  return absl::nullopt;
}

optional<ModelError>
AutofillProfileSyncDifferenceTracker::IncorporateRemoteDelete(
    const std::string& storage_key) {
  DCHECK(!storage_key.empty());
  return DeleteFromLocal(storage_key);
}

optional<ModelError> AutofillProfileSyncDifferenceTracker::FlushToLocal(
    base::OnceClosure autofill_changes_callback) {
  for (const std::string& storage_key : delete_from_local_) {
    if (!table_->RemoveAutofillProfile(
            storage_key, AutofillProfile::Source::kLocalOrSyncable)) {
      return ModelError(FROM_HERE, "Failed deleting from WebDatabase");
    }
  }
  for (const std::unique_ptr<AutofillProfile>& entry : add_to_local_) {
    if (!table_->AddAutofillProfile(*entry)) {
      return ModelError(FROM_HERE, "Failed updating WebDatabase");
    }
  }
  for (const std::unique_ptr<AutofillProfile>& entry : update_to_local_) {
    if (!table_->UpdateAutofillProfile(*entry)) {
      return ModelError(FROM_HERE, "Failed updating WebDatabase");
    }
  }
  if (!delete_from_local_.empty() || !add_to_local_.empty() ||
      !update_to_local_.empty()) {
    std::move(autofill_changes_callback).Run();
  }
  return absl::nullopt;
}

optional<ModelError> AutofillProfileSyncDifferenceTracker::FlushToSync(
    std::vector<std::unique_ptr<AutofillProfile>>* profiles_to_upload_to_sync,
    std::vector<std::string>* profiles_to_delete_from_sync) {
  for (std::unique_ptr<AutofillProfile>& entry : save_to_sync_) {
    profiles_to_upload_to_sync->push_back(std::move(entry));
  }
  for (const std::string& entry : delete_from_sync_) {
    profiles_to_delete_from_sync->push_back(std::move(entry));
  }
  return absl::nullopt;
}

optional<AutofillProfile> AutofillProfileSyncDifferenceTracker::ReadEntry(
    const std::string& storage_key) {
  DCHECK(GetLocalOnlyEntries());
  auto iter = GetLocalOnlyEntries()->find(storage_key);
  if (iter != GetLocalOnlyEntries()->end()) {
    return *iter->second;
  }
  return absl::nullopt;
}

optional<ModelError> AutofillProfileSyncDifferenceTracker::DeleteFromLocal(
    const std::string& storage_key) {
  if (!GetLocalOnlyEntries()) {
    return ModelError(FROM_HERE, "Failed reading from WebDatabase.");
  }
  delete_from_local_.insert(storage_key);
  GetLocalOnlyEntries()->erase(storage_key);
  return absl::nullopt;
}

std::map<std::string, std::unique_ptr<AutofillProfile>>*
AutofillProfileSyncDifferenceTracker::GetLocalOnlyEntries() {
  if (!InitializeLocalOnlyEntriesIfNeeded()) {
    return nullptr;
  }
  return &local_only_entries_;
}

bool AutofillProfileSyncDifferenceTracker::
    InitializeLocalOnlyEntriesIfNeeded() {
  if (local_only_entries_initialized_) {
    return true;
  }

  std::vector<std::unique_ptr<AutofillProfile>> entries;
  if (!table_->GetAutofillProfiles(&entries,
                                   AutofillProfile::Source::kLocalOrSyncable)) {
    return false;
  }

  for (std::unique_ptr<AutofillProfile>& entry : entries) {
    std::string storage_key = GetStorageKeyFromAutofillProfile(*entry);
    local_only_entries_[storage_key] = std::move(entry);
  }

  local_only_entries_initialized_ = true;
  return true;
}

AutofillProfileInitialSyncDifferenceTracker::
    AutofillProfileInitialSyncDifferenceTracker(AutofillTable* table)
    : AutofillProfileSyncDifferenceTracker(table) {}

AutofillProfileInitialSyncDifferenceTracker::
    ~AutofillProfileInitialSyncDifferenceTracker() {}

optional<ModelError>
AutofillProfileInitialSyncDifferenceTracker::IncorporateRemoteDelete(
    const std::string& storage_key) {
  // Remote delete is not allowed in initial sync.
  NOTREACHED();
  return absl::nullopt;
}

optional<ModelError> AutofillProfileInitialSyncDifferenceTracker::FlushToSync(
    std::vector<std::unique_ptr<AutofillProfile>>* profiles_to_upload_to_sync,
    std::vector<std::string>* profiles_to_delete_from_sync) {
  // First, flush standard updates to sync.
  RETURN_IF_ERROR(AutofillProfileSyncDifferenceTracker::FlushToSync(
      profiles_to_upload_to_sync, profiles_to_delete_from_sync));

  // For initial sync, we additionally need to upload all local only entries.
  if (!GetLocalOnlyEntries()) {
    return ModelError(FROM_HERE, "Failed reading from WebDatabase.");
  }
  for (auto& [storage_key, data] : *GetLocalOnlyEntries()) {
    // No deletions coming from remote are allowed for initial sync.
    DCHECK(delete_from_local_.count(storage_key) == 0);
    profiles_to_upload_to_sync->push_back(std::move(data));
  }
  return absl::nullopt;
}

optional<ModelError>
AutofillProfileInitialSyncDifferenceTracker::MergeSimilarEntriesForInitialSync(
    const std::string& app_locale) {
  if (!GetLocalOnlyEntries()) {
    return ModelError(FROM_HERE, "Failed reading from WebDatabase.");
  }

  // This merge cannot happen on the fly during IncorporateRemoteSpecifics().
  // Namely, we do not want to merge a local entry with a _similar_ remote
  // entry if anoter perfectly fitting remote entry comes later during the
  // initial sync (a remote entry fits perfectly to a given local entry if
  // it has fully equal data or even the same storage key). After all the calls
  // to IncorporateRemoteSpecifics() are over, GetLocalOnlyEntries() only
  // contains unmatched entries that can be safely merged with similar remote
  // entries.

  AutofillProfileComparator comparator(app_locale);
  // Loop over all new remote entries to find merge candidates. Using
  // non-const reference because we want to update |remote| in place if
  // needed.
  for (std::unique_ptr<AutofillProfile>& remote : add_to_local_) {
    optional<AutofillProfile> local =
        FindMergeableLocalEntry(*remote, comparator);
    if (!local) {
      continue;
    }

    DVLOG(2)
        << "[AUTOFILL SYNC] A similar profile to "
        << base::UTF16ToUTF8(remote->GetRawInfo(NAME_FIRST))
        << base::UTF16ToUTF8(remote->GetRawInfo(NAME_LAST))
        << " already exists with a different storage key; keep the remote key"
        << GetStorageKeyFromAutofillProfile(*remote)
        << ", merge local data into it and delete the local key"
        << GetStorageKeyFromAutofillProfile(*local);

    // For similar profile pairs, the local profile is always removed and its
    // content merged (if applicable) in the profile that came from sync.
    AutofillProfile remote_before_merge = *remote;
    remote->MergeDataFrom(*local, app_locale);
    if (!remote->EqualsForSyncPurposes(remote_before_merge)) {
      // We need to sync new changes in the entry back to the server.
      save_to_sync_.push_back(std::make_unique<AutofillProfile>(*remote));
      // |remote| is updated in place within |add_to_local_| so the newest
      // merged version is stored to local.
    }

    RETURN_IF_ERROR(DeleteFromLocal(GetStorageKeyFromAutofillProfile(*local)));
  }

  return absl::nullopt;
}

optional<AutofillProfile>
AutofillProfileInitialSyncDifferenceTracker::FindMergeableLocalEntry(
    const AutofillProfile& remote,
    const AutofillProfileComparator& comparator) {
  DCHECK(GetLocalOnlyEntries());

  // Both the remote and the local entry need to be non-verified to be
  // mergeable.
  if (remote.IsVerified()) {
    return absl::nullopt;
  }

  // Check if there is a mergeable local profile.
  for (const auto& [storage_key, local_candidate] : *GetLocalOnlyEntries()) {
    if (!local_candidate->IsVerified() &&
        comparator.AreMergeable(*local_candidate, remote)) {
      return *local_candidate;
    }
  }
  return absl::nullopt;
}

}  // namespace autofill
