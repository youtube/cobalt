// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_LICENSE_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_LICENSE_MANAGER_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/pass_key.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/media/media_license_quota_client.h"
#include "content/common/content_export.h"
#include "media/cdm/cdm_type.h"
#include "media/mojo/mojom/cdm_storage.mojom.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {
class MediaLicenseStorageHost;

// Each StoragePartition owns exactly one instance of this class. This class
// creates and destroys MediaLicenseStorageHost instances to meet the
// demands for CDM from different storage keys.
//
// This class is not thread-safe, and all access to an instance must happen on
// the same sequence.
class CONTENT_EXPORT MediaLicenseManager {
 public:
  // CdmStorage provides per-storage key, per-CDM type storage.
  struct CONTENT_EXPORT BindingContext {
    BindingContext(const blink::StorageKey& storage_key,
                   const media::CdmType& cdm_type)
        : storage_key(storage_key), cdm_type(cdm_type) {}

    const blink::StorageKey storage_key;
    const media::CdmType cdm_type;
  };

  // A CDM file for a given storage key can be uniquely identified by its name
  // and CDM type.
  struct CONTENT_EXPORT CdmFileId {
    CdmFileId(const std::string& name, const media::CdmType& cdm_type);
    CdmFileId(const CdmFileId&);
    ~CdmFileId();

    bool operator==(const CdmFileId& rhs) const {
      return (name == rhs.name) && (cdm_type == rhs.cdm_type);
    }
    bool operator<(const CdmFileId& rhs) const {
      return std::tie(name, cdm_type) < std::tie(rhs.name, rhs.cdm_type);
    }

    const std::string name;
    const media::CdmType cdm_type;
  };

  struct CONTENT_EXPORT CdmFileIdAndContents {
    CdmFileIdAndContents(const CdmFileId& file, std::vector<uint8_t> data);
    CdmFileIdAndContents(const CdmFileIdAndContents&);
    ~CdmFileIdAndContents();

    const CdmFileId file;
    const std::vector<uint8_t> data;
  };

  MediaLicenseManager(
      bool in_memory,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy);
  MediaLicenseManager(const MediaLicenseManager&) = delete;
  MediaLicenseManager& operator=(const MediaLicenseManager&) = delete;
  ~MediaLicenseManager();

  void OpenCdmStorage(const BindingContext& binding_context,
                      mojo::PendingReceiver<media::mojom::CdmStorage> receiver);

  // Called by the MediaLicenseQuotaClient.
  void DeleteBucketData(
      const storage::BucketLocator& bucket,
      storage::mojom::QuotaClient::DeleteBucketDataCallback callback);

  // Returns an empty path if the database is in-memory.
  base::FilePath GetDatabasePath(const storage::BucketLocator& bucket_locator);

  // Called when a receiver is disconnected from a MediaLicenseStorageHost.
  //
  // `host` must be owned by this manager. `host` may be deleted.
  void OnHostReceiverDisconnect(
      MediaLicenseStorageHost* host,
      base::PassKey<MediaLicenseStorageHost> pass_key);

  const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return quota_manager_proxy_;
  }

  const scoped_refptr<base::SequencedTaskRunner>& db_runner() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return db_runner_;
  }

  bool in_memory() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return in_memory_;
  }

 private:
  void DidGetBucket(const blink::StorageKey& storage_key,
                    storage::QuotaErrorOr<storage::BucketInfo> result);

  void DidDeleteBucketData(
      storage::mojom::QuotaClient::DeleteBucketDataCallback callback,
      bool success);

  SEQUENCE_CHECKER(sequence_checker_);

  // Task runner which all database operations are routed through.
  const scoped_refptr<base::SequencedTaskRunner> db_runner_;

  const bool in_memory_;

  // Tracks special rights for apps and extensions, may be null.
  const scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;

  const scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;

  base::flat_map<blink::StorageKey, std::unique_ptr<MediaLicenseStorageHost>>
      hosts_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Maps storage keys to a list of receivers which are awaiting bucket
  // information from the quota system before they can be bound.
  base::flat_map<
      blink::StorageKey,
      std::vector<std::pair<BindingContext,
                            mojo::PendingReceiver<media::mojom::CdmStorage>>>>
      pending_receivers_;

  MediaLicenseQuotaClient quota_client_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Once the QuotaClient receiver is destroyed, the underlying mojo connection
  // is closed. Callbacks associated with mojo calls received over this
  // connection may only be dropped after the connection is closed. For this
  // reason, it's preferable to have the receiver be destroyed as early as
  // possible during the MediaLicenseManager destruction process.
  mojo::Receiver<storage::mojom::QuotaClient> quota_client_receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<MediaLicenseManager> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_LICENSE_MANAGER_H_
