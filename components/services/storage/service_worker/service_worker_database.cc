// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/service_worker/service_worker_database.h"

#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/services/storage/filesystem_proxy_factory.h"
#include "components/services/storage/service_worker/service_worker_database.pb.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom-shared.h"
#include "services/network/public/mojom/cross_origin_opener_policy.mojom-shared.h"
#include "services/network/public/mojom/ip_address_space.mojom-shared.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_ancestor_frame_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_database.mojom.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "url/origin.h"

// LevelDB database schema
// =======================
//
// NOTE
// - int64_t value is serialized as a string by base::NumberToString().
// - GURL value is serialized as a string by GURL::spec().
//
// Version 1 (in sorted order)
//   key: "INITDATA_DB_VERSION"
//   value: "1"
//
//   key: "INITDATA_NEXT_REGISTRATION_ID"
//   value: <int64_t 'next_available_registration_id'>
//
//   key: "INITDATA_NEXT_RESOURCE_ID"
//   value: <int64_t 'next_available_resource_id'>
//
//   key: "INITDATA_NEXT_VERSION_ID"
//   value: <int64_t 'next_available_version_id'>
//
//   Note: This has changed from `GURL origin` to StorageKey but the name will
//   be updated in the future to avoid a migration.
//   TODO(crbug.com/1199077): Update name during a migration to Version 3.
//   See StorageKey::Deserialize() for more information on the format.
//   key: "INITDATA_UNIQUE_ORIGIN:" + <StorageKey>
//   value: <empty>
//
//   key: "PRES:" + <int64_t 'purgeable_resource_id'>
//   value: <empty>
//
//   Note: This has changed from `GURL origin` to StorageKey but the name will
//   be updated in the future to avoid a migration.
//   TODO(crbug.com/1199077): Update name during a migration to Version 3.
//   See StorageKey::Deserialize() for more information on the format.
//   key: "REG:" + <StorageKey> + '\x00' + <int64_t 'registration_id'>
//    (ex. "REG:https://example.com/\x00123456")
//   value: <ServiceWorkerRegistrationData (except for the StorageKey)
//   serialized as a string>
//
//   key: "REG_HAS_USER_DATA:" + <std::string 'user_data_name'> + '\x00'
//            + <int64_t 'registration_id'>
//   value: <empty>
//
//   key: "REG_USER_DATA:" + <int64_t 'registration_id'> + '\x00'
//            + <std::string user_data_name>
//     (ex. "REG_USER_DATA:123456\x00foo_bar")
//   value: <std::string user_data>
//
//   key: "RES:" + <int64_t 'version_id'> + '\x00' + <int64_t 'resource_id'>
//     (ex. "RES:123456\x00654321")
//   value: <ServiceWorkerResourceRecord serialized as a string>
//
//   key: "URES:" + <int64_t 'uncommitted_resource_id'>
//   value: <empty>
//
// Version 2
//
//   Note: This has changed from `GURL origin` to StorageKey but the name will
//   be updated in the future to avoid a migration.
//   TODO(crbug.com/1199077): Update name during a migration to Version 3.
//   See StorageKey::Deserialize() for more information on the format.
//   key: "REGID_TO_ORIGIN:" + <int64_t 'registration_id'>
//   value: <StorageKey>
//
//   OBSOLETE: https://crbug.com/539713
//   key: "INITDATA_DISKCACHE_MIGRATION_NOT_NEEDED"
//   value: <empty>
//     - This entry represents that the diskcache uses the Simple backend and
//       does not have to do diskcache migration (http://crbug.com/487482).
//
//   OBSOLETE: https://crbug.com/539713
//   key: "INITDATA_OLD_DISKCACHE_DELETION_NOT_NEEDED"
//   value: <empty>
//     - This entry represents that the old BlockFile diskcache was deleted
//       after diskcache migration (http://crbug.com/487482).
//
//   OBSOLETE: https://crbug.com/788604
//   key: "INITDATA_FOREIGN_FETCH_ORIGIN:" + <GURL 'origin'>
//   value: <empty>

namespace storage {

namespace service_worker_internals {

const char kDatabaseVersionKey[] = "INITDATA_DB_VERSION";
const char kNextRegIdKey[] = "INITDATA_NEXT_REGISTRATION_ID";
const char kNextResIdKey[] = "INITDATA_NEXT_RESOURCE_ID";
const char kNextVerIdKey[] = "INITDATA_NEXT_VERSION_ID";
const char kUniqueOriginKey[] = "INITDATA_UNIQUE_ORIGIN:";

const char kRegKeyPrefix[] = "REG:";
const char kRegUserDataKeyPrefix[] = "REG_USER_DATA:";
const char kRegHasUserDataKeyPrefix[] = "REG_HAS_USER_DATA:";
const char kRegIdToOriginKeyPrefix[] = "REGID_TO_ORIGIN:";
const char kResKeyPrefix[] = "RES:";
const char kKeySeparator = '\x00';

const char kUncommittedResIdKeyPrefix[] = "URES:";
const char kPurgeableResIdKeyPrefix[] = "PRES:";

const int64_t kCurrentSchemaVersion = 2;

}  // namespace service_worker_internals

namespace {

// The data size is usually small, but the values are changed frequently. So,
// set a low write buffer size to trigger compaction more often.
constexpr size_t kWriteBufferSize = 512 * 1024;

class ServiceWorkerEnv : public leveldb_env::ChromiumEnv {
 public:
  ServiceWorkerEnv()
      : ChromiumEnv("LevelDBEnv.ServiceWorker",
                    storage::CreateFilesystemProxy()) {}

  // Returns a shared instance of ServiceWorkerEnv. This is thread-safe.
  static ServiceWorkerEnv* GetInstance() {
    static base::NoDestructor<ServiceWorkerEnv> instance;
    return instance.get();
  }
};

bool RemovePrefix(const std::string& str,
                  const std::string& prefix,
                  std::string* out) {
  if (!base::StartsWith(str, prefix, base::CompareCase::SENSITIVE))
    return false;
  if (out)
    *out = str.substr(prefix.size());
  return true;
}

std::string CreateRegistrationKeyPrefix(const blink::StorageKey& key) {
  return base::StringPrintf("%s%s%c", service_worker_internals::kRegKeyPrefix,
                            key.Serialize().c_str(),
                            service_worker_internals::kKeySeparator);
}

std::string CreateRegistrationKey(int64_t registration_id,
                                  const blink::StorageKey& key) {
  return CreateRegistrationKeyPrefix(key).append(
      base::NumberToString(registration_id));
}

std::string CreateResourceRecordKeyPrefix(int64_t version_id) {
  return base::StringPrintf("%s%s%c", service_worker_internals::kResKeyPrefix,
                            base::NumberToString(version_id).c_str(),
                            service_worker_internals::kKeySeparator);
}

std::string CreateResourceRecordKey(int64_t version_id, int64_t resource_id) {
  return CreateResourceRecordKeyPrefix(version_id)
      .append(base::NumberToString(resource_id));
}

std::string CreateUniqueOriginKey(const blink::StorageKey& key) {
  return base::StringPrintf("%s%s", service_worker_internals::kUniqueOriginKey,
                            key.Serialize().c_str());
}

std::string CreateResourceIdKey(const char* key_prefix, int64_t resource_id) {
  return base::StringPrintf("%s%s", key_prefix,
                            base::NumberToString(resource_id).c_str());
}

std::string CreateUserDataKeyPrefix(int64_t registration_id) {
  return base::StringPrintf("%s%s%c",
                            service_worker_internals::kRegUserDataKeyPrefix,
                            base::NumberToString(registration_id).c_str(),
                            service_worker_internals::kKeySeparator);
}

std::string CreateUserDataKey(int64_t registration_id,
                              const std::string& user_data_name) {
  return CreateUserDataKeyPrefix(registration_id).append(user_data_name);
}

std::string CreateHasUserDataKeyPrefix(const std::string& user_data_name) {
  return base::StringPrintf(
      "%s%s%c", service_worker_internals::kRegHasUserDataKeyPrefix,
      user_data_name.c_str(), service_worker_internals::kKeySeparator);
}

std::string CreateHasUserDataKey(int64_t registration_id,
                                 const std::string& user_data_name) {
  return CreateHasUserDataKeyPrefix(user_data_name)
      .append(base::NumberToString(registration_id));
}

std::string CreateRegistrationIdToStorageKey(int64_t registration_id) {
  return base::StringPrintf("%s%s",
                            service_worker_internals::kRegIdToOriginKeyPrefix,
                            base::NumberToString(registration_id).c_str());
}

void PutUniqueOriginToBatch(const blink::StorageKey& key,
                            leveldb::WriteBatch* batch) {
  // Value should be empty.
  batch->Put(CreateUniqueOriginKey(key), "");
}

void PutPurgeableResourceIdToBatch(int64_t resource_id,
                                   leveldb::WriteBatch* batch) {
  // Value should be empty.
  batch->Put(
      CreateResourceIdKey(service_worker_internals::kPurgeableResIdKeyPrefix,
                          resource_id),
      "");
}

ServiceWorkerDatabase::Status ParseId(const std::string& serialized,
                                      int64_t* out) {
  DCHECK(out);
  int64_t id;
  if (!base::StringToInt64(serialized, &id) || id < 0)
    return ServiceWorkerDatabase::Status::kErrorCorrupted;
  *out = id;
  return ServiceWorkerDatabase::Status::kOk;
}

ServiceWorkerDatabase::Status LevelDBStatusToServiceWorkerDBStatus(
    const leveldb::Status& status) {
  if (status.ok())
    return ServiceWorkerDatabase::Status::kOk;
  else if (status.IsNotFound())
    return ServiceWorkerDatabase::Status::kErrorNotFound;
  else if (status.IsIOError())
    return ServiceWorkerDatabase::Status::kErrorIOError;
  else if (status.IsCorruption())
    return ServiceWorkerDatabase::Status::kErrorCorrupted;
  else if (status.IsNotSupportedError())
    return ServiceWorkerDatabase::Status::kErrorNotSupported;
  else
    return ServiceWorkerDatabase::Status::kErrorFailed;
}

int64_t AccumulateResourceSizeInBytes(
    const std::vector<mojom::ServiceWorkerResourceRecordPtr>& resources) {
  int64_t total_size_bytes = 0;
  for (const auto& resource : resources)
    total_size_bytes += resource->size_bytes;
  return total_size_bytes;
}

}  // namespace

const char* ServiceWorkerDatabase::StatusToString(
    ServiceWorkerDatabase::Status status) {
  switch (status) {
    case ServiceWorkerDatabase::Status::kOk:
      return "Database OK";
    case ServiceWorkerDatabase::Status::kErrorNotFound:
      return "Database not found";
    case ServiceWorkerDatabase::Status::kErrorIOError:
      return "Database IO error";
    case ServiceWorkerDatabase::Status::kErrorCorrupted:
      return "Database corrupted";
    case ServiceWorkerDatabase::Status::kErrorFailed:
      return "Database operation failed";
    case ServiceWorkerDatabase::Status::kErrorNotSupported:
      return "Database operation not supported";
    case ServiceWorkerDatabase::Status::kErrorDisabled:
      return "Database is disabled";
    case ServiceWorkerDatabase::Status::kErrorStorageDisconnected:
      return "Storage is disconnected";
  }
  NOTREACHED();
  return "Database unknown error";
}

ServiceWorkerDatabase::ServiceWorkerDatabase(const base::FilePath& path)
    : path_(path),
      next_avail_registration_id_(0),
      next_avail_resource_id_(0),
      next_avail_version_id_(0),
      state_(DATABASE_STATE_UNINITIALIZED) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ServiceWorkerDatabase::~ServiceWorkerDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  db_.reset();
}

ServiceWorkerDatabase::DeletedVersion::DeletedVersion() = default;
ServiceWorkerDatabase::DeletedVersion::DeletedVersion(const DeletedVersion&) =
    default;
ServiceWorkerDatabase::DeletedVersion::~DeletedVersion() = default;

ServiceWorkerDatabase::Status ServiceWorkerDatabase::GetNextAvailableIds(
    int64_t* next_avail_registration_id,
    int64_t* next_avail_version_id,
    int64_t* next_avail_resource_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(next_avail_registration_id);
  DCHECK(next_avail_version_id);
  DCHECK(next_avail_resource_id);

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status)) {
    *next_avail_registration_id = 0;
    *next_avail_version_id = 0;
    *next_avail_resource_id = 0;
    return Status::kOk;
  }
  if (status != Status::kOk)
    return status;

  status = ReadNextAvailableId(service_worker_internals::kNextRegIdKey,
                               &next_avail_registration_id_);
  if (status != Status::kOk)
    return status;
  status = ReadNextAvailableId(service_worker_internals::kNextVerIdKey,
                               &next_avail_version_id_);
  if (status != Status::kOk)
    return status;
  status = ReadNextAvailableId(service_worker_internals::kNextResIdKey,
                               &next_avail_resource_id_);
  if (status != Status::kOk)
    return status;

  *next_avail_registration_id = next_avail_registration_id_;
  *next_avail_version_id = next_avail_version_id_;
  *next_avail_resource_id = next_avail_resource_id_;
  return Status::kOk;
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::GetStorageKeysWithRegistrations(
    std::set<blink::StorageKey>* keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(keys->empty());

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kOk;
  if (status != Status::kOk)
    return status;

  {
    std::unique_ptr<leveldb::Iterator> itr(
        db_->NewIterator(leveldb::ReadOptions()));
    for (itr->Seek(service_worker_internals::kUniqueOriginKey); itr->Valid();
         itr->Next()) {
      status = LevelDBStatusToServiceWorkerDBStatus(itr->status());
      if (status != Status::kOk) {
        keys->clear();
        break;
      }

      std::string key_str;
      if (!RemovePrefix(itr->key().ToString(),
                        service_worker_internals::kUniqueOriginKey, &key_str))
        break;

      if (blink::StorageKey::ShouldSkipKeyDueToPartitioning(key_str))
        continue;

      absl::optional<blink::StorageKey> key =
          blink::StorageKey::Deserialize(key_str);
      if (!key) {
        status = Status::kErrorCorrupted;
        keys->clear();
        break;
      }

      keys->insert(*key);
    }
  }

  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::GetRegistrationsForStorageKey(
    const blink::StorageKey& key,
    std::vector<mojom::ServiceWorkerRegistrationDataPtr>* registrations,
    std::vector<std::vector<mojom::ServiceWorkerResourceRecordPtr>>*
        opt_resources_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(registrations->empty());

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kOk;
  if (status != Status::kOk)
    return status;

  std::string prefix = CreateRegistrationKeyPrefix(key);

  // Read all registrations.
  {
    std::unique_ptr<leveldb::Iterator> itr(
        db_->NewIterator(leveldb::ReadOptions()));
    for (itr->Seek(prefix); itr->Valid(); itr->Next()) {
      status = LevelDBStatusToServiceWorkerDBStatus(itr->status());
      if (status != Status::kOk) {
        registrations->clear();
        if (opt_resources_list)
          opt_resources_list->clear();
        break;
      }

      if (!RemovePrefix(itr->key().ToString(), prefix, nullptr))
        break;

      mojom::ServiceWorkerRegistrationDataPtr registration;
      status =
          ParseRegistrationData(itr->value().ToString(), key, &registration);
      if (status != Status::kOk) {
        registrations->clear();
        if (opt_resources_list)
          opt_resources_list->clear();
        break;
      }
      registrations->push_back(std::move(registration));
    }
  }

  // Count reading all registrations as one "read operation" for UMA
  // purposes.
  HandleReadResult(FROM_HERE, status);
  if (status != Status::kOk)
    return status;

  // Read the resources if requested. This must be done after the loop with
  // leveldb::Iterator above, because it calls ReadResouceRecords() which
  // deletes |db_| on failure, and iterators must be destroyed before the
  // database.
  if (opt_resources_list) {
    for (const auto& registration : *registrations) {
      std::vector<mojom::ServiceWorkerResourceRecordPtr> resources;
      // NOTE: ReadResourceRecords already calls HandleReadResult() on its own,
      // so to avoid double-counting the UMA, don't call it again after this.
      status = ReadResourceRecords(*registration, &resources);
      if (status != Status::kOk) {
        registrations->clear();
        opt_resources_list->clear();
        break;
      }
      opt_resources_list->push_back(std::move(resources));
    }
  }

  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::GetUsageForStorageKey(
    const blink::StorageKey& key,
    int64_t& out_usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  out_usage = 0;

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kOk;
  if (status != Status::kOk)
    return status;

  std::string prefix = CreateRegistrationKeyPrefix(key);

  // Read all registrations.
  {
    std::unique_ptr<leveldb::Iterator> itr(
        db_->NewIterator(leveldb::ReadOptions()));
    for (itr->Seek(prefix); itr->Valid(); itr->Next()) {
      status = LevelDBStatusToServiceWorkerDBStatus(itr->status());
      if (status != Status::kOk)
        break;

      if (!RemovePrefix(itr->key().ToString(), prefix, nullptr))
        break;

      mojom::ServiceWorkerRegistrationDataPtr registration;
      status =
          ParseRegistrationData(itr->value().ToString(), key, &registration);
      if (status != Status::kOk)
        break;
      out_usage += registration->resources_total_size_bytes;
    }
  }

  // Count reading all registrations as one "read operation" for UMA
  // purposes.
  HandleReadResult(FROM_HERE, status);
  if (status != Status::kOk) {
    out_usage = 0;
  }

  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::GetAllRegistrations(
    std::vector<mojom::ServiceWorkerRegistrationDataPtr>* registrations) {
  static base::debug::CrashKeyString* crash_key =
      base::debug::AllocateCrashKeyString("num_registrations",
                                          base::debug::CrashKeySize::Size32);

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(registrations->empty());

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kOk;
  if (status != Status::kOk)
    return status;

  {
    std::unique_ptr<leveldb::Iterator> itr(
        db_->NewIterator(leveldb::ReadOptions()));
    for (itr->Seek(service_worker_internals::kRegKeyPrefix); itr->Valid();
         itr->Next()) {
      base::debug::ScopedCrashKeyString num_registrations_crash(
          crash_key, base::NumberToString(registrations->size()));

      status = LevelDBStatusToServiceWorkerDBStatus(itr->status());
      if (status != Status::kOk) {
        registrations->clear();
        break;
      }

      // We need to extract the storage key from the registration key prefix so
      // that we can pass it into ParseRegistrationData below.
      //
      // First remove the prefix and extract the serialized key + separator +
      // registration ID string. (See ' key: "REG:" ' comment at the top of the
      // file for more info).
      std::string prefix_string;
      if (!RemovePrefix(itr->key().ToString(),
                        service_worker_internals::kRegKeyPrefix,
                        &prefix_string))
        break;

      // Now we need to remove the separator + registration ID from the end of
      // the string or else the deserialize step will fail.
      //
      // Find the where the separator is.
      size_t separator_pos =
          prefix_string.find_first_of(service_worker_internals::kKeySeparator);
      if (separator_pos == std::string::npos)
        break;

      // Get only the sub-string before the separator.
      std::string reg_key_string = prefix_string.substr(0, separator_pos);

      if (blink::StorageKey::ShouldSkipKeyDueToPartitioning(reg_key_string))
        continue;

      absl::optional<blink::StorageKey> key =
          blink::StorageKey::Deserialize(reg_key_string);
      if (!key)
        break;

      mojom::ServiceWorkerRegistrationDataPtr registration;
      status =
          ParseRegistrationData(itr->value().ToString(), *key, &registration);
      if (status != Status::kOk) {
        registrations->clear();
        break;
      }
      registrations->push_back(std::move(registration));
    }
  }

  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ReadRegistration(
    int64_t registration_id,
    const blink::StorageKey& key,
    mojom::ServiceWorkerRegistrationDataPtr* registration,
    std::vector<mojom::ServiceWorkerResourceRecordPtr>* resources) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(registration);
  DCHECK(resources);

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kErrorNotFound;
  if (status != Status::kOk)
    return status;

  status = ReadRegistrationData(registration_id, key, registration);
  if (status != Status::kOk)
    return status;

  status = ReadResourceRecords(**registration, resources);
  if (status != Status::kOk)
    return status;

  // ResourceRecord must contain the ServiceWorker's main script.
  if (resources->empty())
    return Status::kErrorCorrupted;

  return Status::kOk;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ReadRegistrationStorageKey(
    int64_t registration_id,
    blink::StorageKey* key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(key);

  Status status = LazyOpen(true);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kErrorNotFound;
  if (status != Status::kOk)
    return status;

  std::string value;
  status = LevelDBStatusToServiceWorkerDBStatus(
      db_->Get(leveldb::ReadOptions(),
               CreateRegistrationIdToStorageKey(registration_id), &value));
  if (status != Status::kOk) {
    HandleReadResult(FROM_HERE,
                     status == Status::kErrorNotFound ? Status::kOk : status);
    return status;
  }

  // If storage partitioning is disabled we shouldn't have any handles to
  // registration IDs associated with partitioned entries.
  DCHECK(!blink::StorageKey::ShouldSkipKeyDueToPartitioning(value));

  absl::optional<blink::StorageKey> parsed =
      blink::StorageKey::Deserialize(value);
  if (!parsed) {
    status = Status::kErrorCorrupted;
    HandleReadResult(FROM_HERE, status);
    return status;
  }

  *key = std::move(*parsed);
  HandleReadResult(FROM_HERE, Status::kOk);
  return Status::kOk;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::WriteRegistration(
    const mojom::ServiceWorkerRegistrationData& registration,
    const std::vector<mojom::ServiceWorkerResourceRecordPtr>& resources,
    DeletedVersion* deleted_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(deleted_version);
  DCHECK(!resources.empty());
  Status status = LazyOpen(true);
  deleted_version->version_id = blink::mojom::kInvalidServiceWorkerVersionId;
  if (status != Status::kOk)
    return status;

  leveldb::WriteBatch batch;
  BumpNextRegistrationIdIfNeeded(registration.registration_id, &batch);
  BumpNextVersionIdIfNeeded(registration.version_id, &batch);

  PutUniqueOriginToBatch(registration.key, &batch);

  DCHECK_EQ(AccumulateResourceSizeInBytes(resources),
            registration.resources_total_size_bytes)
      << "The total size in the registration must match the cumulative "
      << "sizes of the resources.";

  WriteRegistrationDataInBatch(registration, &batch);
  blink::StorageKey key = registration.key;

  batch.Put(CreateRegistrationIdToStorageKey(registration.registration_id),
            key.Serialize());

  // Used for avoiding multiple writes for the same resource id or url.
  std::set<int64_t> pushed_resources;
  std::set<GURL> pushed_urls;
  for (auto itr = resources.begin(); itr != resources.end(); ++itr) {
    if (!(*itr)->url.is_valid())
      return Status::kErrorFailed;

    // Duplicated resource id or url should not exist.
    DCHECK(pushed_resources.insert((*itr)->resource_id).second);
    DCHECK(pushed_urls.insert((*itr)->url).second);

    WriteResourceRecordInBatch(**itr, registration.version_id, &batch);

    // Delete a resource from the uncommitted list.
    batch.Delete(CreateResourceIdKey(
        service_worker_internals::kUncommittedResIdKeyPrefix,
        (*itr)->resource_id));
    // Delete from the purgeable list in case this version was once deleted.
    batch.Delete(
        CreateResourceIdKey(service_worker_internals::kPurgeableResIdKeyPrefix,
                            (*itr)->resource_id));
  }

  // Retrieve a previous version to sweep purgeable resources.
  mojom::ServiceWorkerRegistrationDataPtr old_registration;
  status = ReadRegistrationData(registration.registration_id, registration.key,
                                &old_registration);
  if (status != Status::kOk && status != Status::kErrorNotFound)
    return status;
  if (status == Status::kOk) {
    DCHECK_LT(old_registration->version_id, registration.version_id);
    deleted_version->registration_id = old_registration->registration_id;
    deleted_version->version_id = old_registration->version_id;
    deleted_version->resources_total_size_bytes =
        old_registration->resources_total_size_bytes;
    status = DeleteResourceRecords(old_registration->version_id,
                                   &deleted_version->newly_purgeable_resources,
                                   &batch);
    if (status != Status::kOk)
      return status;

    // Currently resource sharing across versions and registrations is not
    // supported, so resource ids should not be overlapped between
    // |registration| and |old_registration|.
    std::set<int64_t> deleted_resources(
        deleted_version->newly_purgeable_resources.begin(),
        deleted_version->newly_purgeable_resources.end());
    DCHECK(base::STLSetIntersection<std::set<int64_t>>(pushed_resources,
                                                       deleted_resources)
               .empty());
  }

  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::UpdateVersionToActive(
    int64_t registration_id,
    const blink::StorageKey& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kErrorNotFound;
  if (status != Status::kOk)
    return status;
  if (key.origin().opaque())
    return Status::kErrorFailed;

  mojom::ServiceWorkerRegistrationDataPtr registration;
  status = ReadRegistrationData(registration_id, key, &registration);
  if (status != Status::kOk)
    return status;

  registration->is_active = true;

  leveldb::WriteBatch batch;
  WriteRegistrationDataInBatch(*registration, &batch);
  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::UpdateLastCheckTime(
    int64_t registration_id,
    const blink::StorageKey& key,
    const base::Time& time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kErrorNotFound;
  if (status != Status::kOk)
    return status;
  if (key.origin().opaque())
    return Status::kErrorFailed;

  mojom::ServiceWorkerRegistrationDataPtr registration;
  status = ReadRegistrationData(registration_id, key, &registration);
  if (status != Status::kOk)
    return status;

  registration->last_update_check = time;

  leveldb::WriteBatch batch;
  WriteRegistrationDataInBatch(*registration, &batch);
  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::UpdateNavigationPreloadEnabled(
    int64_t registration_id,
    const blink::StorageKey& key,
    bool enable) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kErrorNotFound;
  if (status != Status::kOk)
    return status;
  if (key.origin().opaque())
    return Status::kErrorFailed;

  mojom::ServiceWorkerRegistrationDataPtr registration;
  status = ReadRegistrationData(registration_id, key, &registration);
  if (status != Status::kOk)
    return status;

  registration->navigation_preload_state->enabled = enable;

  leveldb::WriteBatch batch;
  WriteRegistrationDataInBatch(*registration, &batch);
  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::UpdateNavigationPreloadHeader(
    int64_t registration_id,
    const blink::StorageKey& key,
    const std::string& value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kErrorNotFound;
  if (status != Status::kOk)
    return status;
  if (key.origin().opaque())
    return Status::kErrorFailed;

  mojom::ServiceWorkerRegistrationDataPtr registration;
  status = ReadRegistrationData(registration_id, key, &registration);
  if (status != Status::kOk)
    return status;

  registration->navigation_preload_state->header = value;

  leveldb::WriteBatch batch;
  WriteRegistrationDataInBatch(*registration, &batch);
  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::UpdateFetchHandlerType(
    int64_t registration_id,
    const blink::StorageKey& key,
    const blink::mojom::ServiceWorkerFetchHandlerType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kErrorNotFound;
  if (status != Status::kOk)
    return status;
  if (key.origin().opaque())
    return Status::kErrorFailed;

  mojom::ServiceWorkerRegistrationDataPtr registration;
  status = ReadRegistrationData(registration_id, key, &registration);
  if (status != Status::kOk)
    return status;

  registration->fetch_handler_type = type;

  leveldb::WriteBatch batch;
  WriteRegistrationDataInBatch(*registration, &batch);
  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::UpdateResourceSha256Checksums(
    int64_t registration_id,
    const blink::StorageKey& key,
    const base::flat_map<int64_t, std::string>& updated_sha256_checksums) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  leveldb::WriteBatch batch;

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status)) {
    return Status::kErrorNotFound;
  }
  if (status != Status::kOk) {
    return status;
  }

  mojom::ServiceWorkerRegistrationDataPtr registration;
  status = ReadRegistrationData(registration_id, key, &registration);
  if (status != Status::kOk) {
    return status;
  }

  std::vector<mojom::ServiceWorkerResourceRecordPtr> resources;
  status = ReadResourceRecords(*registration, &resources);
  if (status != Status::kOk) {
    return status;
  }

  std::set<int64_t> updated_resource_ids;
  for (const auto& resource : resources) {
    if (!updated_resource_ids.insert(resource->resource_id).second) {
      // The database wrongly contains the same resource id.
      return Status::kErrorCorrupted;
    }
    auto itr = updated_sha256_checksums.find(resource->resource_id);
    if (itr == updated_sha256_checksums.end()) {
      return Status::kErrorNotFound;
    }
    resource->sha256_checksum = itr->second;
    WriteResourceRecordInBatch(*resource, registration->version_id, &batch);
  }

  // Check if all updated_sha256_checksums are used.
  if (updated_resource_ids.size() != updated_sha256_checksums.size()) {
    return Status::kErrorNotFound;
  }

  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::DeleteRegistration(
    int64_t registration_id,
    const blink::StorageKey& key,
    DeletedVersion* deleted_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(deleted_version);
  deleted_version->version_id = blink::mojom::kInvalidServiceWorkerVersionId;
  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kOk;
  if (status != Status::kOk)
    return status;
  if (key.origin().opaque())
    return Status::kErrorFailed;

  leveldb::WriteBatch batch;

  // Remove |key| from unique origins if a registration specified by
  // |registration_id| is the only one for |key|.
  // TODO(nhiroki): Check the uniqueness by more efficient way.
  std::vector<mojom::ServiceWorkerRegistrationDataPtr> registrations;
  status = GetRegistrationsForStorageKey(key, &registrations, nullptr);
  if (status != Status::kOk)
    return status;

  if (registrations.size() == 1 &&
      registrations[0]->registration_id == registration_id) {
    batch.Delete(CreateUniqueOriginKey(key));
  }

  // Delete a registration specified by |registration_id|.
  batch.Delete(CreateRegistrationKey(registration_id, key));
  batch.Delete(CreateRegistrationIdToStorageKey(registration_id));

  // Delete resource records and user data associated with the registration.
  for (const auto& registration : registrations) {
    if (registration->registration_id == registration_id) {
      deleted_version->registration_id = registration_id;
      deleted_version->version_id = registration->version_id;
      deleted_version->resources_total_size_bytes =
          registration->resources_total_size_bytes;
      status = DeleteResourceRecords(
          registration->version_id, &deleted_version->newly_purgeable_resources,
          &batch);
      if (status != Status::kOk)
        return status;

      status = DeleteUserDataForRegistration(registration_id, &batch);
      if (status != Status::kOk)
        return status;
      break;
    }
  }

  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ReadUserData(
    int64_t registration_id,
    const std::vector<std::string>& user_data_names,
    std::vector<std::string>* user_data_values) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, registration_id);
  DCHECK(!user_data_names.empty());
  DCHECK(user_data_values);

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kErrorNotFound;
  if (status != Status::kOk)
    return status;

  user_data_values->resize(user_data_names.size());
  for (size_t i = 0; i < user_data_names.size(); i++) {
    const std::string key =
        CreateUserDataKey(registration_id, user_data_names[i]);
    status = LevelDBStatusToServiceWorkerDBStatus(
        db_->Get(leveldb::ReadOptions(), key, &(*user_data_values)[i]));
    if (status != Status::kOk) {
      user_data_values->clear();
      break;
    }
  }
  HandleReadResult(FROM_HERE,
                   status == Status::kErrorNotFound ? Status::kOk : status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ReadUserDataByKeyPrefix(
    int64_t registration_id,
    const std::string& user_data_name_prefix,
    std::vector<std::string>* user_data_values) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, registration_id);
  DCHECK(user_data_values);

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kErrorNotFound;
  if (status != Status::kOk)
    return status;

  std::string prefix =
      CreateUserDataKey(registration_id, user_data_name_prefix);
  {
    std::unique_ptr<leveldb::Iterator> itr(
        db_->NewIterator(leveldb::ReadOptions()));
    for (itr->Seek(prefix); itr->Valid(); itr->Next()) {
      status = LevelDBStatusToServiceWorkerDBStatus(itr->status());
      if (status != Status::kOk) {
        user_data_values->clear();
        break;
      }

      if (!itr->key().starts_with(prefix))
        break;

      std::string user_data_value;
      status = LevelDBStatusToServiceWorkerDBStatus(
          db_->Get(leveldb::ReadOptions(), itr->key(), &user_data_value));
      if (status != Status::kOk) {
        user_data_values->clear();
        break;
      }

      user_data_values->push_back(user_data_value);
    }
  }

  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::ReadUserKeysAndDataByKeyPrefix(
    int64_t registration_id,
    const std::string& user_data_name_prefix,
    base::flat_map<std::string, std::string>* user_data_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, registration_id);
  DCHECK(user_data_map);

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kErrorNotFound;
  if (status != Status::kOk)
    return status;

  std::string prefix =
      CreateUserDataKey(registration_id, user_data_name_prefix);
  {
    std::unique_ptr<leveldb::Iterator> itr(
        db_->NewIterator(leveldb::ReadOptions()));
    for (itr->Seek(prefix); itr->Valid(); itr->Next()) {
      status = LevelDBStatusToServiceWorkerDBStatus(itr->status());
      if (status != Status::kOk) {
        user_data_map->clear();
        break;
      }

      if (!itr->key().starts_with(prefix))
        break;

      std::string user_data_value;
      status = LevelDBStatusToServiceWorkerDBStatus(
          db_->Get(leveldb::ReadOptions(), itr->key(), &user_data_value));
      if (status != Status::kOk) {
        user_data_map->clear();
        break;
      }

      leveldb::Slice s = itr->key();
      s.remove_prefix(prefix.size());
      // Always insert at the end of the map as they're retrieved presorted from
      // the database.
      user_data_map->insert(user_data_map->end(),
                            {s.ToString(), user_data_value});
    }
  }

  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::WriteUserData(
    int64_t registration_id,
    const blink::StorageKey& key,
    const std::vector<mojom::ServiceWorkerUserDataPtr>& user_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, registration_id);
  DCHECK(!user_data.empty());

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kErrorNotFound;
  if (status != Status::kOk)
    return status;

  // There should be the registration specified by |registration_id|.
  mojom::ServiceWorkerRegistrationDataPtr registration;
  status = ReadRegistrationData(registration_id, key, &registration);
  if (status != Status::kOk)
    return status;

  leveldb::WriteBatch batch;
  for (const auto& entry : user_data) {
    DCHECK(!entry->key.empty());
    batch.Put(CreateUserDataKey(registration_id, entry->key), entry->value);
    batch.Put(CreateHasUserDataKey(registration_id, entry->key), "");
  }
  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::DeleteUserData(
    int64_t registration_id,
    const std::vector<std::string>& user_data_names) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, registration_id);
  DCHECK(!user_data_names.empty());

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kOk;
  if (status != Status::kOk)
    return status;

  leveldb::WriteBatch batch;
  for (const std::string& name : user_data_names) {
    DCHECK(!name.empty());
    batch.Delete(CreateUserDataKey(registration_id, name));
    batch.Delete(CreateHasUserDataKey(registration_id, name));
  }
  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::DeleteUserDataByKeyPrefixes(
    int64_t registration_id,
    const std::vector<std::string>& user_data_name_prefixes) {
  // Example |user_data_name_prefixes| is {"abc", "xyz"}.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, registration_id);

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kOk;
  if (status != Status::kOk)
    return status;

  // Example |key_prefix_without_user_data_name_prefix| is
  // "REG_USER_DATA:123456\x00".
  std::string key_prefix_without_user_data_name_prefix =
      CreateUserDataKeyPrefix(registration_id);

  leveldb::WriteBatch batch;

  for (const std::string& user_data_name_prefix : user_data_name_prefixes) {
    // Example |key_prefix| is "REG_USER_DATA:123456\x00abc".
    std::string key_prefix =
        CreateUserDataKey(registration_id, user_data_name_prefix);
    std::unique_ptr<leveldb::Iterator> itr(
        db_->NewIterator(leveldb::ReadOptions()));
    for (itr->Seek(key_prefix); itr->Valid(); itr->Next()) {
      status = LevelDBStatusToServiceWorkerDBStatus(itr->status());
      if (status != Status::kOk)
        return status;

      // Example |itr->key()| is "REG_USER_DATA:123456\x00abcdef".
      if (!itr->key().starts_with(key_prefix)) {
        // |itr| reached the end of the range of keys prefixed by |key_prefix|.
        break;
      }

      // Example |user_data_name| is "abcdef".
      std::string user_data_name;
      bool did_remove_prefix = RemovePrefix(
          itr->key().ToString(), key_prefix_without_user_data_name_prefix,
          &user_data_name);
      DCHECK(did_remove_prefix) << "starts_with already matched longer prefix";

      batch.Delete(itr->key());
      // Example |CreateHasUserDataKey| is "REG_HAS_USER_DATA:abcdef\x00123456".
      batch.Delete(CreateHasUserDataKey(registration_id, user_data_name));
    }
  }

  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::RewriteDB() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kOk;
  if (status != Status::kOk)
    return status;
  if (IsDatabaseInMemory())
    return Status::kOk;

  leveldb_env::Options options;
  options.create_if_missing = true;
  options.env = ServiceWorkerEnv::GetInstance();
  options.write_buffer_size = kWriteBufferSize;

  status = LevelDBStatusToServiceWorkerDBStatus(
      leveldb_env::RewriteDB(options, path_.AsUTF8Unsafe(), &db_));
  return status;
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::ReadUserDataForAllRegistrations(
    const std::string& user_data_name,
    std::vector<mojom::ServiceWorkerUserDataPtr>* user_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(user_data->empty());

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kOk;
  if (status != Status::kOk)
    return status;

  std::string key_prefix = CreateHasUserDataKeyPrefix(user_data_name);
  {
    std::unique_ptr<leveldb::Iterator> itr(
        db_->NewIterator(leveldb::ReadOptions()));
    for (itr->Seek(key_prefix); itr->Valid(); itr->Next()) {
      status = LevelDBStatusToServiceWorkerDBStatus(itr->status());
      if (status != Status::kOk) {
        user_data->clear();
        break;
      }

      std::string registration_id_string;
      if (!RemovePrefix(itr->key().ToString(), key_prefix,
                        &registration_id_string)) {
        break;
      }

      int64_t registration_id;
      status = ParseId(registration_id_string, &registration_id);
      if (status != Status::kOk) {
        user_data->clear();
        break;
      }

      std::string value;
      status = LevelDBStatusToServiceWorkerDBStatus(
          db_->Get(leveldb::ReadOptions(),
                   CreateUserDataKey(registration_id, user_data_name), &value));
      if (status != Status::kOk) {
        user_data->clear();
        break;
      }
      user_data->emplace_back(mojom::ServiceWorkerUserData::New(
          registration_id, user_data_name, value));
    }
  }

  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::ReadUserDataForAllRegistrationsByKeyPrefix(
    const std::string& user_data_name_prefix,
    std::vector<mojom::ServiceWorkerUserDataPtr>* user_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(user_data->empty());

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kOk;
  if (status != Status::kOk)
    return status;

  std::string key_prefix = service_worker_internals::kRegHasUserDataKeyPrefix +
                           user_data_name_prefix;
  {
    std::unique_ptr<leveldb::Iterator> itr(
        db_->NewIterator(leveldb::ReadOptions()));
    for (itr->Seek(key_prefix); itr->Valid(); itr->Next()) {
      status = LevelDBStatusToServiceWorkerDBStatus(itr->status());
      if (status != Status::kOk) {
        user_data->clear();
        break;
      }

      if (!itr->key().starts_with(key_prefix))
        break;

      std::string user_data_name_with_id;
      if (!RemovePrefix(itr->key().ToString(),
                        service_worker_internals::kRegHasUserDataKeyPrefix,
                        &user_data_name_with_id)) {
        break;
      }

      std::vector<std::string> parts = base::SplitString(
          user_data_name_with_id,
          std::string(1, service_worker_internals::kKeySeparator),
          base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
      if (parts.size() != 2) {
        status = Status::kErrorCorrupted;
        user_data->clear();
        break;
      }

      int64_t registration_id;
      status = ParseId(parts[1], &registration_id);
      if (status != Status::kOk) {
        user_data->clear();
        break;
      }

      std::string value;
      status = LevelDBStatusToServiceWorkerDBStatus(
          db_->Get(leveldb::ReadOptions(),
                   CreateUserDataKey(registration_id, parts[0]), &value));
      if (status != Status::kOk) {
        user_data->clear();
        break;
      }
      user_data->push_back(
          mojom::ServiceWorkerUserData::New(registration_id, parts[0], value));
    }
  }

  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::DeleteUserDataForAllRegistrationsByKeyPrefix(
    const std::string& user_data_name_prefix) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kOk;
  if (status != Status::kOk)
    return status;

  leveldb::WriteBatch batch;
  std::string key_prefix = service_worker_internals::kRegHasUserDataKeyPrefix +
                           user_data_name_prefix;

  std::unique_ptr<leveldb::Iterator> itr(
      db_->NewIterator(leveldb::ReadOptions()));
  for (itr->Seek(key_prefix); itr->Valid(); itr->Next()) {
    status = LevelDBStatusToServiceWorkerDBStatus(itr->status());
    if (status != Status::kOk)
      return status;

    if (!itr->key().starts_with(key_prefix)) {
      // |itr| reached the end of the range of keys prefixed by |key_prefix|.
      break;
    }

    std::string user_data_name_with_id;
    bool did_remove_prefix =
        RemovePrefix(itr->key().ToString(),
                     service_worker_internals::kRegHasUserDataKeyPrefix,
                     &user_data_name_with_id);
    DCHECK(did_remove_prefix);

    std::vector<std::string> parts = base::SplitString(
        user_data_name_with_id,
        std::string(1, service_worker_internals::kKeySeparator),
        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    if (parts.size() != 2)
      return Status::kErrorCorrupted;

    int64_t registration_id;
    status = ParseId(parts[1], &registration_id);
    if (status != Status::kOk)
      return status;

    batch.Delete(itr->key());
    batch.Delete(CreateUserDataKey(registration_id, parts[0]));
  }

  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::GetUncommittedResourceIds(
    std::vector<int64_t>* ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ReadResourceIds(service_worker_internals::kUncommittedResIdKeyPrefix,
                         ids);
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::WriteUncommittedResourceIds(
    const std::vector<int64_t>& ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  leveldb::WriteBatch batch;
  Status status = WriteResourceIdsInBatch(
      service_worker_internals::kUncommittedResIdKeyPrefix, ids, &batch);
  if (status != Status::kOk)
    return status;
  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::GetPurgeableResourceIds(
    std::vector<int64_t>* ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ReadResourceIds(service_worker_internals::kPurgeableResIdKeyPrefix,
                         ids);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ClearPurgeableResourceIds(
    const std::vector<int64_t>& ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kOk;

  leveldb::WriteBatch batch;
  status = DeleteResourceIdsInBatch(
      service_worker_internals::kPurgeableResIdKeyPrefix, ids, &batch);
  if (status != Status::kOk)
    return status;
  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::PurgeUncommittedResourceIds(
    const std::vector<int64_t>& ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kOk;

  leveldb::WriteBatch batch;
  status = DeleteResourceIdsInBatch(
      service_worker_internals::kUncommittedResIdKeyPrefix, ids, &batch);
  if (status != Status::kOk)
    return status;
  status = WriteResourceIdsInBatch(
      service_worker_internals::kPurgeableResIdKeyPrefix, ids, &batch);
  if (status != Status::kOk)
    return status;
  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::DeleteAllDataForOrigins(
    const std::set<url::Origin>& origins,
    std::vector<int64_t>* newly_purgeable_resources) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kOk;
  if (status != Status::kOk)
    return status;
  leveldb::WriteBatch batch;

  std::vector<mojom::ServiceWorkerRegistrationDataPtr> registrations;
  status = GetAllRegistrations(&registrations);
  if (status != Status::kOk) {
    return status;
  }

  // Filter all registrations, using the criteria in the doc comment to
  // determine which keys match.
  for (const mojom::ServiceWorkerRegistrationDataPtr& reg : registrations) {
    blink::StorageKey& key = reg->key;

    for (const url::Origin& requested_origin : origins) {
      if (requested_origin.opaque()) {
        return Status::kErrorFailed;
      }

      auto match = key.origin() == requested_origin;
      match = match ||
              (key.IsThirdPartyContext() &&
               key.top_level_site() == net::SchemefulSite(requested_origin));
      if (!match) {
        continue;
      }

      batch.Delete(CreateUniqueOriginKey(key));
      batch.Delete(CreateRegistrationKey(reg->registration_id, key));
      batch.Delete(CreateRegistrationIdToStorageKey(reg->registration_id));

      status = DeleteResourceRecords(reg->version_id, newly_purgeable_resources,
                                     &batch);
      if (status != Status::kOk)
        return status;

      status = DeleteUserDataForRegistration(reg->registration_id, &batch);
      if (status != Status::kOk)
        return status;
    }
  }

  return WriteBatch(&batch);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::DestroyDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Disable(FROM_HERE, Status::kOk);

  if (IsDatabaseInMemory()) {
    env_.reset();
    return Status::kOk;
  }

  Status status = LevelDBStatusToServiceWorkerDBStatus(
      leveldb_chrome::DeleteDB(path_, leveldb_env::Options()));

  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.Database.DestroyDatabaseResult",
                            status);

  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::LazyOpen(
    bool create_if_missing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Do not try to open a database if we tried and failed once.
  if (state_ == DATABASE_STATE_DISABLED)
    return Status::kErrorFailed;
  if (IsOpen())
    return Status::kOk;

  if (!create_if_missing &&
      (IsDatabaseInMemory() ||
       !leveldb_chrome::PossiblyValidDB(path_, leveldb::Env::Default()))) {
    // Avoid opening a database if it does not exist at the |path_|.
    return Status::kErrorNotFound;
  }

  leveldb_env::Options options;
  options.create_if_missing = create_if_missing;
  if (IsDatabaseInMemory()) {
    env_ = leveldb_chrome::NewMemEnv("service-worker");
    options.env = env_.get();
  } else {
    options.env = ServiceWorkerEnv::GetInstance();
  }
  options.write_buffer_size = kWriteBufferSize;

  Status status = LevelDBStatusToServiceWorkerDBStatus(
      leveldb_env::OpenDB(options, path_.AsUTF8Unsafe(), &db_));
  HandleOpenResult(FROM_HERE, status);
  if (status != Status::kOk) {
    // TODO(nhiroki): Should we retry to open the database?
    return status;
  }

  int64_t db_version;
  status = ReadDatabaseVersion(&db_version);
  if (status != Status::kOk)
    return status;

  switch (db_version) {
    case 0:
      // This database is new. It will be initialized when something is written.
      DCHECK_EQ(DATABASE_STATE_UNINITIALIZED, state_);
      return Status::kOk;
    case 1:
      // This database has an obsolete schema version. ServiceWorkerStorage
      // should recreate it.
      status = Status::kErrorFailed;
      Disable(FROM_HERE, status);
      return status;
    case 2:
      DCHECK_EQ(db_version, service_worker_internals::kCurrentSchemaVersion);
      state_ = DATABASE_STATE_INITIALIZED;
      return Status::kOk;
    default:
      // Other cases should be handled in ReadDatabaseVersion.
      NOTREACHED();
      return Status::kErrorCorrupted;
  }
}

bool ServiceWorkerDatabase::IsNewOrNonexistentDatabase(
    ServiceWorkerDatabase::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status == Status::kErrorNotFound)
    return true;
  if (status == Status::kOk && state_ == DATABASE_STATE_UNINITIALIZED)
    return true;
  return false;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ReadNextAvailableId(
    const char* id_key,
    int64_t* next_avail_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(id_key);
  DCHECK(next_avail_id);

  std::string value;
  Status status = LevelDBStatusToServiceWorkerDBStatus(
      db_->Get(leveldb::ReadOptions(), id_key, &value));
  if (status == Status::kErrorNotFound) {
    // Nobody has gotten the next resource id for |id_key|.
    *next_avail_id = 0;
    HandleReadResult(FROM_HERE, Status::kOk);
    return Status::kOk;
  } else if (status != Status::kOk) {
    HandleReadResult(FROM_HERE, status);
    return status;
  }

  status = ParseId(value, next_avail_id);
  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ReadRegistrationData(
    int64_t registration_id,
    const blink::StorageKey& key,
    mojom::ServiceWorkerRegistrationDataPtr* registration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(registration);

  const std::string registration_key =
      CreateRegistrationKey(registration_id, key);
  std::string value;
  Status status = LevelDBStatusToServiceWorkerDBStatus(
      db_->Get(leveldb::ReadOptions(), registration_key, &value));
  if (status != Status::kOk) {
    HandleReadResult(FROM_HERE,
                     status == Status::kErrorNotFound ? Status::kOk : status);
    return status;
  }

  status = ParseRegistrationData(value, key, registration);
  HandleReadResult(FROM_HERE, status);
  return status;
}

network::mojom::ReferrerPolicy ConvertReferrerPolicyFromProtocolBufferToMojom(
    ServiceWorkerRegistrationData::ReferrerPolicyValue value) {
  switch (value) {
    case ServiceWorkerRegistrationData::DEFAULT:
      return network::mojom::ReferrerPolicy::kDefault;
    case ServiceWorkerRegistrationData::ALWAYS:
      return network::mojom::ReferrerPolicy::kAlways;
    case ServiceWorkerRegistrationData::NO_REFERRER_WHEN_DOWNGRADE:
      return network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade;
    case ServiceWorkerRegistrationData::NEVER:
      return network::mojom::ReferrerPolicy::kNever;
    case ServiceWorkerRegistrationData::ORIGIN:
      return network::mojom::ReferrerPolicy::kOrigin;
    case ServiceWorkerRegistrationData::ORIGIN_WHEN_CROSS_ORIGIN:
      return network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin;
    case ServiceWorkerRegistrationData::STRICT_ORIGIN_WHEN_CROSS_ORIGIN:
      return network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin;
    case ServiceWorkerRegistrationData::SAME_ORIGIN:
      return network::mojom::ReferrerPolicy::kSameOrigin;
    case ServiceWorkerRegistrationData::STRICT_ORIGIN:
      return network::mojom::ReferrerPolicy::kStrictOrigin;
  }
}

network::mojom::IPAddressSpace ConvertIPAddressSpaceFromProtocolBufferToMojom(
    ServiceWorkerRegistrationData::IPAddressSpace value) {
  switch (value) {
    case ServiceWorkerRegistrationData::LOOPBACK:
      return network::mojom::IPAddressSpace::kLoopback;
    case ServiceWorkerRegistrationData::LOCAL:
      return network::mojom::IPAddressSpace::kLocal;
    case ServiceWorkerRegistrationData::PUBLIC:
      return network::mojom::IPAddressSpace::kPublic;
    case ServiceWorkerRegistrationData::UNKNOWN:
      return network::mojom::IPAddressSpace::kUnknown;
  }
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ParseRegistrationData(
    const std::string& serialized,
    const blink::StorageKey& key,
    mojom::ServiceWorkerRegistrationDataPtr* out) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(out);
  ServiceWorkerRegistrationData data;
  if (!data.ParseFromString(serialized))
    return Status::kErrorCorrupted;

  GURL scope_url(data.scope_url());
  GURL script_url(data.script_url());
  if (!scope_url.is_valid() || !script_url.is_valid() ||
      scope_url.DeprecatedGetOriginAsURL() !=
          script_url.DeprecatedGetOriginAsURL() ||
      key.origin() != url::Origin::Create(scope_url)) {
    DLOG(ERROR) << "Scope URL '" << data.scope_url() << "' and/or script url '"
                << data.script_url() << "' and/or the storage key's origin '"
                << key.origin() << "' are invalid or have mismatching origins.";
    return Status::kErrorCorrupted;
  }

  if (data.registration_id() >= next_avail_registration_id_ ||
      data.version_id() >= next_avail_version_id_) {
    // The stored registration should not have the higher registration id or
    // version id than the next available id.
    DLOG(ERROR) << "Registration id " << data.registration_id()
                << " and/or version id " << data.version_id()
                << " is higher than the next available id.";
    return Status::kErrorCorrupted;
  }

  // Convert ServiceWorkerRegistrationData to RegistrationData.
  *out = mojom::ServiceWorkerRegistrationData::New();
  (*out)->registration_id = data.registration_id();
  (*out)->scope = scope_url;
  (*out)->script = script_url;
  (*out)->key = key;
  (*out)->version_id = data.version_id();
  (*out)->is_active = data.is_active();
  // The old protobuf may not have fetch_handler_type.
  (*out)->fetch_handler_type =
      (data.has_fetch_handler())
          ? blink::mojom::ServiceWorkerFetchHandlerType::kNotSkippable
          : blink::mojom::ServiceWorkerFetchHandlerType::kNoHandler;
  if (data.has_fetch_handler_skippable_type()) {
    if (!data.has_fetch_handler()) {
      DLOG(ERROR)
          << "has_fetch_handler must be true if fetch_handler_skippable_type"
          << " is set.";
      return Status::kErrorCorrupted;
    }
    if (!ServiceWorkerRegistrationData_FetchHandlerSkippableType_IsValid(
            data.fetch_handler_skippable_type())) {
      DLOG(ERROR) << "Fetch handler type '"
                  << data.fetch_handler_skippable_type() << "' is not valid.";
      return Status::kErrorCorrupted;
    }
    switch (data.fetch_handler_skippable_type()) {
      case ServiceWorkerRegistrationData::NOT_SKIPPABLE:
        (*out)->fetch_handler_type =
            blink::mojom::ServiceWorkerFetchHandlerType::kNotSkippable;
        break;
      case ServiceWorkerRegistrationData::SKIPPABLE_EMPTY_FETCH_HANDLER:
        (*out)->fetch_handler_type =
            blink::mojom::ServiceWorkerFetchHandlerType::kEmptyFetchHandler;
        break;
        // TODO(crbug.com/1347319): implement other fetch_handler_type.
    }
  }
  (*out)->last_update_check = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(data.last_update_check_time()));
  (*out)->resources_total_size_bytes = data.resources_total_size_bytes();
  if (data.has_origin_trial_tokens()) {
    const ServiceWorkerOriginTrialInfo& info = data.origin_trial_tokens();
    FeatureToTokensMap origin_trial_tokens;
    for (int i = 0; i < info.features_size(); ++i) {
      const auto& feature = info.features(i);
      for (int j = 0; j < feature.tokens_size(); ++j)
        origin_trial_tokens[feature.name()].push_back(feature.tokens(j));
    }
    (*out)->origin_trial_tokens = origin_trial_tokens;
  }

  (*out)->navigation_preload_state =
      blink::mojom::NavigationPreloadState::New();
  if (data.has_navigation_preload_state()) {
    const ServiceWorkerNavigationPreloadState& state =
        data.navigation_preload_state();
    (*out)->navigation_preload_state->enabled = state.enabled();
    if (state.has_header())
      (*out)->navigation_preload_state->header = state.header();
  }

  for (uint32_t feature : data.used_features()) {
    // Add features that are valid WebFeature values. Invalid values can
    // legitimately exist on disk when a version of Chrome had the feature and
    // wrote the data but the value was removed from the WebFeature enum in a
    // later version of Chrome.
    auto web_feature = static_cast<blink::mojom::WebFeature>(feature);
    if (IsKnownEnumValue(web_feature))
      (*out)->used_features.push_back(web_feature);
  }

  if (data.has_script_type()) {
    auto value = data.script_type();
    if (!ServiceWorkerRegistrationData_ServiceWorkerScriptType_IsValid(value)) {
      DLOG(ERROR) << "Worker script type '" << value << "' is not valid.";
      return Status::kErrorCorrupted;
    }
    (*out)->script_type = static_cast<blink::mojom::ScriptType>(value);
  }

  if (data.has_script_response_time()) {
    (*out)->script_response_time = base::Time::FromDeltaSinceWindowsEpoch(
        base::Microseconds(data.script_response_time()));
  }

  if (data.has_update_via_cache()) {
    auto value = data.update_via_cache();
    if (!ServiceWorkerRegistrationData_ServiceWorkerUpdateViaCacheType_IsValid(
            value)) {
      DLOG(ERROR) << "Update via cache mode '" << value << "' is not valid.";
      return Status::kErrorCorrupted;
    }
    (*out)->update_via_cache =
        static_cast<blink::mojom::ServiceWorkerUpdateViaCache>(value);
  }

  if (data.has_cross_origin_embedder_policy_value()) {
    if (!(*out)->policy_container_policies) {
      (*out)->policy_container_policies =
          blink::mojom::PolicyContainerPolicies::New();
    }
    if (!ServiceWorkerRegistrationData::CrossOriginEmbedderPolicyValue_IsValid(
            data.cross_origin_embedder_policy_value())) {
      DLOG(ERROR)
          << "Cross origin embedder policy in policy container policies '"
          << data.cross_origin_embedder_policy_value() << "' is not valid.";
      return Status::kErrorCorrupted;
    }
    switch (data.cross_origin_embedder_policy_value()) {
      case ServiceWorkerRegistrationData::REQUIRE_CORP:
        (*out)->policy_container_policies->cross_origin_embedder_policy.value =
            network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;
        break;
      case ServiceWorkerRegistrationData::CREDENTIALLESS:
        (*out)->policy_container_policies->cross_origin_embedder_policy.value =
            network::mojom::CrossOriginEmbedderPolicyValue::kCredentialless;
        break;
      case ServiceWorkerRegistrationData::NONE_OR_NOT_EXIST:
        (*out)->policy_container_policies->cross_origin_embedder_policy.value =
            network::mojom::CrossOriginEmbedderPolicyValue::kNone;
    }
  }

  if (data.has_cross_origin_embedder_policy_reporting_endpoint()) {
    (*out)
        ->policy_container_policies->cross_origin_embedder_policy
        .reporting_endpoint =
        data.cross_origin_embedder_policy_reporting_endpoint();
  }

  if (data.has_cross_origin_embedder_policy_report_only_value()) {
    switch (data.cross_origin_embedder_policy_report_only_value()) {
      case ServiceWorkerRegistrationData::REQUIRE_CORP:
        (*out)
            ->policy_container_policies->cross_origin_embedder_policy
            .report_only_value =
            network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;
        break;
      case ServiceWorkerRegistrationData::CREDENTIALLESS:
        (*out)
            ->policy_container_policies->cross_origin_embedder_policy
            .report_only_value =
            network::mojom::CrossOriginEmbedderPolicyValue::kCredentialless;
        break;
      case ServiceWorkerRegistrationData::NONE_OR_NOT_EXIST:
        (*out)
            ->policy_container_policies->cross_origin_embedder_policy
            .report_only_value =
            network::mojom::CrossOriginEmbedderPolicyValue::kNone;
    }
  }

  if (data.has_cross_origin_embedder_policy_report_only_reporting_endpoint()) {
    (*out)
        ->policy_container_policies->cross_origin_embedder_policy
        .report_only_reporting_endpoint =
        data.cross_origin_embedder_policy_report_only_reporting_endpoint();
  }

  if (data.has_ancestor_frame_type()) {
    if (!ServiceWorkerRegistrationData_AncestorFrameType_IsValid(
            data.ancestor_frame_type())) {
      DLOG(ERROR) << "Ancestor frame type '" << data.ancestor_frame_type()
                  << "' is not valid.";
      return Status::kErrorCorrupted;
    }
    switch (data.ancestor_frame_type()) {
      case ServiceWorkerRegistrationData::NORMAL_FRAME:
        (*out)->ancestor_frame_type =
            blink::mojom::AncestorFrameType::kNormalFrame;
        break;
      case ServiceWorkerRegistrationData::FENCED_FRAME:
        (*out)->ancestor_frame_type =
            blink::mojom::AncestorFrameType::kFencedFrame;
        break;
    }
  }

  if (data.has_policy_container_policies()) {
    if (!(*out)->policy_container_policies) {
      (*out)->policy_container_policies =
          blink::mojom::PolicyContainerPolicies::New();
    }
    auto& policies = data.policy_container_policies();
    if (policies.has_referrer_policy()) {
      if (!ServiceWorkerRegistrationData::ReferrerPolicyValue_IsValid(
              policies.referrer_policy())) {
        DLOG(ERROR) << "Referrer policy in policy container policies '"
                    << policies.referrer_policy() << "' is not valid.";
        return Status::kErrorCorrupted;
      }
      (*out)->policy_container_policies->referrer_policy =
          ConvertReferrerPolicyFromProtocolBufferToMojom(
              policies.referrer_policy());
    }
    if (policies.has_sandbox_flags()) {
      (*out)->policy_container_policies->sandbox_flags =
          static_cast<network::mojom::WebSandboxFlags>(
              policies.sandbox_flags());
    }
    if (policies.has_ip_address_space()) {
      if (!ServiceWorkerRegistrationData_IPAddressSpace_IsValid(
              policies.ip_address_space())) {
        DLOG(ERROR) << "IP address space in policy container policies '"
                    << policies.ip_address_space() << "' is not valid.";
        return Status::kErrorCorrupted;
      }
      (*out)->policy_container_policies->ip_address_space =
          ConvertIPAddressSpaceFromProtocolBufferToMojom(
              policies.ip_address_space());
    }
  }

  return Status::kOk;
}

ServiceWorkerRegistrationData::CrossOriginEmbedderPolicyValue
ConvertCrossOriginEmbedderPolicyValueFromMojomToProtocolBuffer(
    network::mojom::CrossOriginEmbedderPolicyValue value) {
  switch (value) {
    case network::mojom::CrossOriginEmbedderPolicyValue::kNone:
      return ServiceWorkerRegistrationData::NONE_OR_NOT_EXIST;
    case network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp:
      return ServiceWorkerRegistrationData::REQUIRE_CORP;
    case network::mojom::CrossOriginEmbedderPolicyValue::kCredentialless:
      return ServiceWorkerRegistrationData::CREDENTIALLESS;
  }
}

ServiceWorkerRegistrationData::ReferrerPolicyValue
ConvertReferrerPolicyFromMojomToProtocolBuffer(
    network::mojom::ReferrerPolicy value) {
  switch (value) {
    case network::mojom::ReferrerPolicy::kDefault:
      return ServiceWorkerRegistrationData::DEFAULT;
    case network::mojom::ReferrerPolicy::kAlways:
      return ServiceWorkerRegistrationData::ALWAYS;
    case network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade:
      return ServiceWorkerRegistrationData::NO_REFERRER_WHEN_DOWNGRADE;
    case network::mojom::ReferrerPolicy::kNever:
      return ServiceWorkerRegistrationData::NEVER;
    case network::mojom::ReferrerPolicy::kOrigin:
      return ServiceWorkerRegistrationData::ORIGIN;
    case network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin:
      return ServiceWorkerRegistrationData::ORIGIN_WHEN_CROSS_ORIGIN;
    case network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin:
      return ServiceWorkerRegistrationData::STRICT_ORIGIN_WHEN_CROSS_ORIGIN;
    case network::mojom::ReferrerPolicy::kSameOrigin:
      return ServiceWorkerRegistrationData::SAME_ORIGIN;
    case network::mojom::ReferrerPolicy::kStrictOrigin:
      return ServiceWorkerRegistrationData::STRICT_ORIGIN;
  }
}

ServiceWorkerRegistrationData::IPAddressSpace
ConvertIPAddressSpaceFromMojomToProtocolBuffer(
    network::mojom::IPAddressSpace value) {
  switch (value) {
    case network::mojom::IPAddressSpace::kLoopback:
      return ServiceWorkerRegistrationData::LOOPBACK;
    case network::mojom::IPAddressSpace::kLocal:
      return ServiceWorkerRegistrationData::LOCAL;
    case network::mojom::IPAddressSpace::kPublic:
      return ServiceWorkerRegistrationData::PUBLIC;
    case network::mojom::IPAddressSpace::kUnknown:
      return ServiceWorkerRegistrationData::UNKNOWN;
  }
}

void ServiceWorkerDatabase::WriteRegistrationDataInBatch(
    const mojom::ServiceWorkerRegistrationData& registration,
    leveldb::WriteBatch* batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(batch);

  // The registration id and version id should be bumped before this.
  DCHECK_GT(next_avail_registration_id_, registration.registration_id);
  DCHECK_GT(next_avail_version_id_, registration.version_id);

  // Convert RegistrationData to ServiceWorkerRegistrationData.
  ServiceWorkerRegistrationData data;
  data.set_registration_id(registration.registration_id);
  data.set_scope_url(registration.scope.spec());
  data.set_script_url(registration.script.spec());
  // Do not store the StorageKey, it's already encoded in the registration key
  // prefix.
  data.set_version_id(registration.version_id);
  data.set_is_active(registration.is_active);
  data.set_has_fetch_handler(
      registration.fetch_handler_type !=
      blink::mojom::ServiceWorkerFetchHandlerType::kNoHandler);
  if (data.has_fetch_handler()) {
    switch (registration.fetch_handler_type) {
      case blink::mojom::ServiceWorkerFetchHandlerType::kNotSkippable:
        data.set_fetch_handler_skippable_type(
            ServiceWorkerRegistrationData::NOT_SKIPPABLE);
        break;
      case blink::mojom::ServiceWorkerFetchHandlerType::kEmptyFetchHandler:
        data.set_fetch_handler_skippable_type(
            ServiceWorkerRegistrationData::SKIPPABLE_EMPTY_FETCH_HANDLER);
        break;
      // TODO(crbug.com/1347319): implement other fetch_handler_type.
      // TODO(crbug.com/1351246): remove default if possible.
      default:
        DCHECK(false) << "Unknown fetch_handler_type is used."
                      << registration.fetch_handler_type;
    }
  }
  data.set_last_update_check_time(
      registration.last_update_check.ToDeltaSinceWindowsEpoch()
          .InMicroseconds());
  data.set_script_response_time(
      registration.script_response_time.ToDeltaSinceWindowsEpoch()
          .InMicroseconds());
  data.set_resources_total_size_bytes(registration.resources_total_size_bytes);
  if (registration.origin_trial_tokens) {
    ServiceWorkerOriginTrialInfo* info = data.mutable_origin_trial_tokens();
    for (const auto& feature : *registration.origin_trial_tokens) {
      ServiceWorkerOriginTrialFeature* feature_out = info->add_features();
      feature_out->set_name(feature.first);
      for (const auto& token : feature.second)
        feature_out->add_tokens(token);
    }
  }
  ServiceWorkerNavigationPreloadState* state =
      data.mutable_navigation_preload_state();
  if (registration.navigation_preload_state) {
    state->set_enabled(registration.navigation_preload_state->enabled);
    state->set_header(registration.navigation_preload_state->header);
  } else {
    state->set_enabled(false);
  }

  for (blink::mojom::WebFeature web_feature : registration.used_features)
    data.add_used_features(static_cast<uint32_t>(web_feature));

  data.set_script_type(
      static_cast<ServiceWorkerRegistrationData_ServiceWorkerScriptType>(
          registration.script_type));
  data.set_update_via_cache(
      static_cast<
          ServiceWorkerRegistrationData_ServiceWorkerUpdateViaCacheType>(
          registration.update_via_cache));

  switch (registration.ancestor_frame_type) {
    case blink::mojom::AncestorFrameType::kNormalFrame:
      data.set_ancestor_frame_type(ServiceWorkerRegistrationData::NORMAL_FRAME);
      break;
    case blink::mojom::AncestorFrameType::kFencedFrame:
      data.set_ancestor_frame_type(ServiceWorkerRegistrationData::FENCED_FRAME);
      break;
  }

  if (registration.policy_container_policies) {
    data.set_cross_origin_embedder_policy_value(
        ConvertCrossOriginEmbedderPolicyValueFromMojomToProtocolBuffer(
            registration.policy_container_policies->cross_origin_embedder_policy
                .value));

    if (registration.policy_container_policies->cross_origin_embedder_policy
            .reporting_endpoint) {
      data.set_cross_origin_embedder_policy_reporting_endpoint(
          registration.policy_container_policies->cross_origin_embedder_policy
              .reporting_endpoint.value());
    }
    data.set_cross_origin_embedder_policy_report_only_value(
        ConvertCrossOriginEmbedderPolicyValueFromMojomToProtocolBuffer(
            registration.policy_container_policies->cross_origin_embedder_policy
                .report_only_value));
    if (registration.policy_container_policies->cross_origin_embedder_policy
            .report_only_reporting_endpoint) {
      data.set_cross_origin_embedder_policy_report_only_reporting_endpoint(
          registration.policy_container_policies->cross_origin_embedder_policy
              .report_only_reporting_endpoint.value());
    }

    ServiceWorkerRegistrationData::PolicyContainerPolicies* policies =
        data.mutable_policy_container_policies();
    policies->set_referrer_policy(
        ConvertReferrerPolicyFromMojomToProtocolBuffer(
            registration.policy_container_policies->referrer_policy));
    policies->set_sandbox_flags(static_cast<int>(
        registration.policy_container_policies->sandbox_flags));
    policies->set_ip_address_space(
        ConvertIPAddressSpaceFromMojomToProtocolBuffer(
            registration.policy_container_policies->ip_address_space));
  }

  std::string value;
  bool success = data.SerializeToString(&value);
  DCHECK(success);
  blink::StorageKey key = registration.key;
  batch->Put(CreateRegistrationKey(data.registration_id(), key), value);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ReadResourceRecords(
    const mojom::ServiceWorkerRegistrationData& registration,
    std::vector<mojom::ServiceWorkerResourceRecordPtr>* resources) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(resources->empty());

  Status status = Status::kOk;
  bool has_main_resource = false;
  const std::string prefix =
      CreateResourceRecordKeyPrefix(registration.version_id);
  {
    std::unique_ptr<leveldb::Iterator> itr(
        db_->NewIterator(leveldb::ReadOptions()));
    for (itr->Seek(prefix); itr->Valid(); itr->Next()) {
      status = LevelDBStatusToServiceWorkerDBStatus(itr->status());
      if (status != Status::kOk) {
        resources->clear();
        break;
      }

      if (!RemovePrefix(itr->key().ToString(), prefix, nullptr))
        break;

      mojom::ServiceWorkerResourceRecordPtr resource;
      status = ParseResourceRecord(itr->value().ToString(), &resource);
      if (status != Status::kOk) {
        resources->clear();
        break;
      }

      if (registration.script == resource->url) {
        DCHECK(!has_main_resource);
        has_main_resource = true;
      }

      resources->push_back(std::move(resource));
    }
  }

  // |resources| should contain the main script.
  if (!has_main_resource) {
    resources->clear();
    status = Status::kErrorCorrupted;
  }

  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ParseResourceRecord(
    const std::string& serialized,
    mojom::ServiceWorkerResourceRecordPtr* out) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(out);
  ServiceWorkerResourceRecord record;
  if (!record.ParseFromString(serialized))
    return Status::kErrorCorrupted;

  GURL url(record.url());
  if (!url.is_valid())
    return Status::kErrorCorrupted;

  if (record.resource_id() >= next_avail_resource_id_) {
    // The stored resource should not have a higher resource id than the next
    // available resource id.
    return Status::kErrorCorrupted;
  }

  // Convert ServiceWorkerResourceRecord to ResourceRecord.
  *out = mojom::ServiceWorkerResourceRecord::New();
  (*out)->resource_id = record.resource_id();
  (*out)->url = url;
  (*out)->size_bytes = record.size_bytes();
  if (record.has_sha256_checksum()) {
    (*out)->sha256_checksum = record.sha256_checksum();
  }
  return Status::kOk;
}

void ServiceWorkerDatabase::WriteResourceRecordInBatch(
    const mojom::ServiceWorkerResourceRecord& resource,
    int64_t version_id,
    leveldb::WriteBatch* batch) {
  DCHECK(batch);
  DCHECK_GE(resource.size_bytes, 0);

  // The next available resource id should be bumped when a resource is recorded
  // in the uncommitted list and this should be nop. However, we attempt it here
  // for some unit tests that bypass WriteUncommittedResourceIds() when setting
  // up a dummy registration. Otherwise, tests fail due to a corruption error in
  // ParseResourceRecord().
  // TODO(nhiroki): Remove this hack by making tests appropriately set up
  // uncommitted resource ids before writing a registration.
  BumpNextResourceIdIfNeeded(resource.resource_id, batch);

  // Convert ResourceRecord to ServiceWorkerResourceRecord.
  ServiceWorkerResourceRecord data;
  data.set_resource_id(resource.resource_id);
  data.set_url(resource.url.spec());
  data.set_size_bytes(resource.size_bytes);
  if (resource.sha256_checksum) {
    data.set_sha256_checksum(*resource.sha256_checksum);
  }

  std::string value;
  bool success = data.SerializeToString(&value);
  DCHECK(success);
  batch->Put(CreateResourceRecordKey(version_id, data.resource_id()), value);
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::DeleteResourceRecords(
    int64_t version_id,
    std::vector<int64_t>* newly_purgeable_resources,
    leveldb::WriteBatch* batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(batch);

  Status status = Status::kOk;
  const std::string prefix = CreateResourceRecordKeyPrefix(version_id);

  {
    std::unique_ptr<leveldb::Iterator> itr(
        db_->NewIterator(leveldb::ReadOptions()));
    for (itr->Seek(prefix); itr->Valid(); itr->Next()) {
      status = LevelDBStatusToServiceWorkerDBStatus(itr->status());
      if (status != Status::kOk)
        break;

      const std::string key = itr->key().ToString();
      std::string unprefixed;
      if (!RemovePrefix(key, prefix, &unprefixed))
        break;

      int64_t resource_id;
      status = ParseId(unprefixed, &resource_id);
      if (status != Status::kOk)
        break;

      // Remove a resource record.
      batch->Delete(key);

      // Currently resource sharing across versions and registrations is not
      // supported, so we can purge this without caring about it.
      PutPurgeableResourceIdToBatch(resource_id, batch);
      newly_purgeable_resources->push_back(resource_id);
    }
  }

  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ReadResourceIds(
    const char* id_key_prefix,
    std::vector<int64_t>* ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(id_key_prefix);
  DCHECK(ids->empty());

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kOk;
  if (status != Status::kOk)
    return status;

  {
    std::set<int64_t> unique_ids;
    std::unique_ptr<leveldb::Iterator> itr(
        db_->NewIterator(leveldb::ReadOptions()));
    for (itr->Seek(id_key_prefix); itr->Valid(); itr->Next()) {
      status = LevelDBStatusToServiceWorkerDBStatus(itr->status());
      if (status != Status::kOk) {
        unique_ids.clear();
        break;
      }

      std::string unprefixed;
      if (!RemovePrefix(itr->key().ToString(), id_key_prefix, &unprefixed))
        break;

      int64_t resource_id;
      status = ParseId(unprefixed, &resource_id);
      if (status != Status::kOk) {
        unique_ids.clear();
        break;
      }
      unique_ids.insert(resource_id);
    }
    *ids = std::vector<int64_t>(unique_ids.begin(), unique_ids.end());
  }

  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::WriteResourceIdsInBatch(
    const char* id_key_prefix,
    const std::vector<int64_t>& ids,
    leveldb::WriteBatch* batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(id_key_prefix);

  Status status = LazyOpen(true);
  if (status != Status::kOk)
    return status;

  if (ids.empty())
    return Status::kOk;
  for (auto itr = ids.begin(); itr != ids.end(); ++itr) {
    // Value should be empty.
    batch->Put(CreateResourceIdKey(id_key_prefix, *itr), "");
  }
  // std::set is sorted, so the last element is the largest.
  BumpNextResourceIdIfNeeded(*ids.rbegin(), batch);
  return Status::kOk;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::DeleteResourceIdsInBatch(
    const char* id_key_prefix,
    const std::vector<int64_t>& ids,
    leveldb::WriteBatch* batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(id_key_prefix);

  Status status = LazyOpen(false);
  if (IsNewOrNonexistentDatabase(status))
    return Status::kOk;
  if (status != Status::kOk)
    return status;

  for (auto itr = ids.begin(); itr != ids.end(); ++itr) {
    batch->Delete(CreateResourceIdKey(id_key_prefix, *itr));
  }
  return Status::kOk;
}

ServiceWorkerDatabase::Status
ServiceWorkerDatabase::DeleteUserDataForRegistration(
    int64_t registration_id,
    leveldb::WriteBatch* batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(batch);
  Status status = Status::kOk;
  const std::string prefix = CreateUserDataKeyPrefix(registration_id);

  {
    std::unique_ptr<leveldb::Iterator> itr(
        db_->NewIterator(leveldb::ReadOptions()));
    for (itr->Seek(prefix); itr->Valid(); itr->Next()) {
      status = LevelDBStatusToServiceWorkerDBStatus(itr->status());
      if (status != Status::kOk)
        break;

      const std::string key = itr->key().ToString();
      std::string user_data_name;
      if (!RemovePrefix(key, prefix, &user_data_name))
        break;
      batch->Delete(key);
      batch->Delete(CreateHasUserDataKey(registration_id, user_data_name));
    }
  }

  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::ReadDatabaseVersion(
    int64_t* db_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string value;
  Status status = LevelDBStatusToServiceWorkerDBStatus(
      db_->Get(leveldb::ReadOptions(),
               service_worker_internals::kDatabaseVersionKey, &value));
  if (status == Status::kErrorNotFound) {
    // The database hasn't been initialized yet.
    *db_version = 0;
    HandleReadResult(FROM_HERE, Status::kOk);
    return Status::kOk;
  }

  if (status != Status::kOk) {
    HandleReadResult(FROM_HERE, status);
    return status;
  }

  const int kFirstValidVersion = 1;
  if (!base::StringToInt64(value, db_version) ||
      *db_version < kFirstValidVersion ||
      service_worker_internals::kCurrentSchemaVersion < *db_version) {
    status = Status::kErrorCorrupted;
    HandleReadResult(FROM_HERE, status);
    return status;
  }

  status = Status::kOk;
  HandleReadResult(FROM_HERE, status);
  return status;
}

ServiceWorkerDatabase::Status ServiceWorkerDatabase::WriteBatch(
    leveldb::WriteBatch* batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(batch);
  DCHECK_NE(DATABASE_STATE_DISABLED, state_);

  if (state_ == DATABASE_STATE_UNINITIALIZED) {
    // Write database default values.
    batch->Put(
        service_worker_internals::kDatabaseVersionKey,
        base::NumberToString(service_worker_internals::kCurrentSchemaVersion));
    state_ = DATABASE_STATE_INITIALIZED;
  }

  Status status = LevelDBStatusToServiceWorkerDBStatus(
      db_->Write(leveldb::WriteOptions(), batch));
  HandleWriteResult(FROM_HERE, status);
  return status;
}

void ServiceWorkerDatabase::BumpNextRegistrationIdIfNeeded(
    int64_t used_id,
    leveldb::WriteBatch* batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(batch);
  if (next_avail_registration_id_ <= used_id) {
    next_avail_registration_id_ = used_id + 1;
    batch->Put(service_worker_internals::kNextRegIdKey,
               base::NumberToString(next_avail_registration_id_));
  }
}

void ServiceWorkerDatabase::BumpNextResourceIdIfNeeded(
    int64_t used_id,
    leveldb::WriteBatch* batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(batch);
  if (next_avail_resource_id_ <= used_id) {
    next_avail_resource_id_ = used_id + 1;
    batch->Put(service_worker_internals::kNextResIdKey,
               base::NumberToString(next_avail_resource_id_));
  }
}

void ServiceWorkerDatabase::BumpNextVersionIdIfNeeded(
    int64_t used_id,
    leveldb::WriteBatch* batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(batch);
  if (next_avail_version_id_ <= used_id) {
    next_avail_version_id_ = used_id + 1;
    batch->Put(service_worker_internals::kNextVerIdKey,
               base::NumberToString(next_avail_version_id_));
  }
}

bool ServiceWorkerDatabase::IsOpen() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_ != nullptr;
}

void ServiceWorkerDatabase::Disable(const base::Location& from_here,
                                    Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status != Status::kOk) {
    DLOG(ERROR) << "Failed at: " << from_here.ToString()
                << " with error: " << StatusToString(status);
    DLOG(ERROR) << "ServiceWorkerDatabase is disabled.";
  }
  state_ = DATABASE_STATE_DISABLED;
  db_.reset();
}

void ServiceWorkerDatabase::HandleOpenResult(const base::Location& from_here,
                                             Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status != Status::kOk)
    Disable(from_here, status);

  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.Database.OpenResult", status);
}

void ServiceWorkerDatabase::HandleReadResult(const base::Location& from_here,
                                             Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status != Status::kOk)
    Disable(from_here, status);

  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.Database.ReadResult", status);
}

void ServiceWorkerDatabase::HandleWriteResult(const base::Location& from_here,
                                              Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status != Status::kOk)
    Disable(from_here, status);

  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.Database.WriteResult", status);
}

bool ServiceWorkerDatabase::IsDatabaseInMemory() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return path_.empty();
}

}  // namespace storage
