// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DATA_MANAGER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DATA_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "content/browser/background_fetch/background_fetch.pb.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_scheduler.h"
#include "content/browser/background_fetch/storage/database_task.h"
#include "content/browser/background_fetch/storage/get_initialization_data_task.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"

namespace storage {
class BlobDataHandle;
class QuotaManagerProxy;
}  // namespace storage

namespace content {

class BackgroundFetchDataManagerObserver;
class BackgroundFetchRequestInfo;
class BackgroundFetchRequestMatchParams;
class ChromeBlobStorageContext;
class ServiceWorkerContextWrapper;
class StoragePartitionImpl;

// The BackgroundFetchDataManager is a wrapper around persistent storage (the
// Service Worker database), exposing APIs for the read and write queries needed
// for Background Fetch.
//
// There must only be a single instance of this class per StoragePartition, and
// it must only be used on the UI thread, since it relies on there being no
// other code concurrently reading/writing the Background Fetch keys of the same
// Service Worker database (except for deletions, e.g. it's safe for the Service
// Worker code to remove a ServiceWorkerRegistration and all its keys).
//
// Storage schema is documented in storage/README.md
class CONTENT_EXPORT BackgroundFetchDataManager
    : public background_fetch::DatabaseTaskHost {
 public:
  using GetInitializationDataCallback = base::OnceCallback<void(
      blink::mojom::BackgroundFetchError,
      std::vector<background_fetch::BackgroundFetchInitializationData>)>;
  using SettledFetchesCallback = base::OnceCallback<void(
      blink::mojom::BackgroundFetchError,
      std::vector<blink::mojom::BackgroundFetchSettledFetchPtr>)>;
  using CreateRegistrationCallback = base::OnceCallback<void(
      blink::mojom::BackgroundFetchError,
      blink::mojom::BackgroundFetchRegistrationDataPtr)>;
  using GetRegistrationCallback = base::OnceCallback<void(
      blink::mojom::BackgroundFetchError,
      BackgroundFetchRegistrationId,
      blink::mojom::BackgroundFetchRegistrationDataPtr)>;
  using MarkRegistrationForDeletionCallback =
      base::OnceCallback<void(blink::mojom::BackgroundFetchError,
                              blink::mojom::BackgroundFetchFailureReason)>;
  using GetRequestBlobCallback =
      base::OnceCallback<void(blink::mojom::BackgroundFetchError,
                              blink::mojom::SerializedBlobPtr)>;
  using MarkRequestCompleteCallback =
      base::OnceCallback<void(blink::mojom::BackgroundFetchError)>;
  using NextRequestCallback =
      base::OnceCallback<void(blink::mojom::BackgroundFetchError,
                              scoped_refptr<BackgroundFetchRequestInfo>)>;

  BackgroundFetchDataManager(
      base::WeakPtr<StoragePartitionImpl> storage_partition,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy);

  BackgroundFetchDataManager(const BackgroundFetchDataManager&) = delete;
  BackgroundFetchDataManager& operator=(const BackgroundFetchDataManager&) =
      delete;

  ~BackgroundFetchDataManager() override;

  // Grabs a reference to CacheStorageManager.
  virtual void Initialize();

  // Adds or removes the given |observer| to this data manager instance.
  void AddObserver(BackgroundFetchDataManagerObserver* observer);
  void RemoveObserver(BackgroundFetchDataManagerObserver* observer);

  // Gets the required data to initialize BackgroundFetchContext with the
  // appropriate JobControllers. This will be called when BackgroundFetchContext
  // is being initialized.
  void GetInitializationData(GetInitializationDataCallback callback);

  // Creates and stores a new registration with the given properties. Will
  // invoke the |callback| when the registration has been created, which may
  // fail due to invalid input or storage errors.
  void CreateRegistration(
      const BackgroundFetchRegistrationId& registration_id,
      std::vector<blink::mojom::FetchAPIRequestPtr> requests,
      blink::mojom::BackgroundFetchOptionsPtr options,
      const SkBitmap& icon,
      bool start_paused,
      const net::IsolationInfo& isolation_info,
      CreateRegistrationCallback callback);

  // Get the BackgroundFetchRegistration.
  void GetRegistration(int64_t service_worker_registration_id,
                       const blink::StorageKey& storage_key,
                       const std::string& developer_id,
                       GetRegistrationCallback callback);

  // Reads the settled fetches for the given |registration_id| based on
  // |match_params|. Both the Request and Response objects will be initialised
  // based on the stored data. Will invoke the |callback| when the list of
  // fetches has been compiled.
  void MatchRequests(
      const BackgroundFetchRegistrationId& registration_id,
      std::unique_ptr<BackgroundFetchRequestMatchParams> match_params,
      SettledFetchesCallback callback);

  // Retrieves the next pending request for |registration_id| and invoke
  // |callback| with it.
  void PopNextRequest(const BackgroundFetchRegistrationId& registration_id,
                      NextRequestCallback callback);

  // Retrieves the request blob associated with |request_info|. THis should be
  // called for requests that are known to have a blob.
  void GetRequestBlob(
      const BackgroundFetchRegistrationId& registration_id,
      const scoped_refptr<BackgroundFetchRequestInfo>& request_info,
      GetRequestBlobCallback callback);

  // Marks |request_info| as complete and calls |callback| when done.
  void MarkRequestAsComplete(
      const BackgroundFetchRegistrationId& registration_id,
      scoped_refptr<BackgroundFetchRequestInfo> request_info,
      MarkRequestCompleteCallback callback);

  // Marks that the
  // backgroundfetchsuccess/backgroundfetchfail/backgroundfetchabort event is
  // being dispatched. It's not possible to call DeleteRegistration at this
  // point as JavaScript may hold a reference to a BackgroundFetchRegistration
  // object and we need to keep the corresponding data around until the last
  // such reference is released (or until shutdown). We can't just move the
  // Background Fetch registration's data to RAM as it might consume too much
  // memory. So instead this step disassociates the |developer_id| from the
  // |unique_id|, so that existing JS objects with a reference to |unique_id|
  // can still access the data, but it can no longer be reached using GetIds or
  // GetRegistration. If |check_for_failure| is true, the task will also check
  // whether there is any associated failure reason with the fetches. This
  // helps figure out whether a success or fail event should be dispatched.
  void MarkRegistrationForDeletion(
      const BackgroundFetchRegistrationId& registration_id,
      bool check_for_failure,
      MarkRegistrationForDeletionCallback callback);

  // Deletes the registration identified by |registration_id|. Should only be
  // called once the refcount of JavaScript BackgroundFetchRegistration objects
  // referring to this registration drops to zero. Will invoke the |callback|
  // when the registration has been deleted from storage.
  void DeleteRegistration(const BackgroundFetchRegistrationId& registration_id,
                          HandleBackgroundFetchErrorCallback callback);

  // List all Background Fetch registration |developer_id|s for a Service
  // Worker.
  void GetDeveloperIdsForServiceWorker(
      int64_t service_worker_registration_id,
      const blink::StorageKey& storage_key,
      blink::mojom::BackgroundFetchService::GetDeveloperIdsCallback callback);

  const base::ObserverList<BackgroundFetchDataManagerObserver>::Unchecked&
  observers() {
    return observers_;
  }

  void Shutdown();

 private:
  FRIEND_TEST_ALL_PREFIXES(BackgroundFetchDataManagerTest, Cleanup);
  friend class BackgroundFetchDataManagerTest;
  friend class BackgroundFetchTestDataManager;
  friend class background_fetch::DatabaseTask;

  // Accessors for tests and DatabaseTasks.
  ServiceWorkerContextWrapper* service_worker_context() const {
    return service_worker_context_.get();
  }
  std::set<std::string>& ref_counted_unique_ids() {
    return ref_counted_unique_ids_;
  }
  ChromeBlobStorageContext* blob_storage_context() const {
    return blob_storage_context_.get();
  }
  const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy() const {
    return quota_manager_proxy_;
  }

  void AddDatabaseTask(std::unique_ptr<background_fetch::DatabaseTask> task);

  // DatabaseTaskHost implementation.
  void OnTaskFinished(background_fetch::DatabaseTask* task) override;
  BackgroundFetchDataManager* data_manager() override;
  base::WeakPtr<background_fetch::DatabaseTaskHost> GetWeakPtr() override;

  void Cleanup();

  // Get a CacheStorage remote for the given |storage_key| and |unique_id|. This
  // will either be a reference to an existing remote or will cause the
  // CacheStorage to be opened.  The BackgroundFetchDataManager owns this
  // remote for the lifetime of the connection.
  mojo::Remote<blink::mojom::CacheStorage>& GetOrOpenCacheStorage(
      const blink::StorageKey& storage_key,
      const std::string& unique_id);
  void OpenCache(const blink::StorageKey& storage_key,
                 const std::string& unique_id,
                 int64_t trace_id,
                 blink::mojom::CacheStorage::OpenCallback callback);
  void DeleteCache(const blink::StorageKey& storage_key,
                   const std::string& unique_id,
                   int64_t trace_id,
                   blink::mojom::CacheStorage::DeleteCallback callback);
  void DidDeleteCache(const std::string& unique_id,
                      blink::mojom::CacheStorage::DeleteCallback callback,
                      blink::mojom::CacheStorageError result);

  void HasCache(const blink::StorageKey& storage_key,
                const std::string& unique_id,
                int64_t trace_id,
                blink::mojom::CacheStorage::HasCallback callback);

  // Whether Shutdown was called on BackgroundFetchContext.
  bool shutting_down_ = false;

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  base::WeakPtr<StoragePartitionImpl> storage_partition_;

  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;

  // The blob storage request with which response information will be stored.
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;

  // Pending database operations, serialized to ensure consistency.
  // Invariant: the frontmost task, if any, has already been started.
  base::queue<std::unique_ptr<background_fetch::DatabaseTask>> database_tasks_;

  base::ObserverList<BackgroundFetchDataManagerObserver>::Unchecked observers_;

  // The |unique_id|s of registrations that have been deactivated since the
  // browser was last started. They will be automatically deleted when the
  // refcount of JavaScript objects that refers to them goes to zero, unless
  // the browser is shutdown first.
  std::set<std::string> ref_counted_unique_ids_;

  // A map of open CacheStorage remotes keyed by the registration
  // |unique_id|. These remotes are created opportunistically in
  // GetOrOpenCacheStorage(). They are cleared after the Cache has been
  // deleted.
  // TODO(crbug.com/711354): Possibly update key when CORS support is added.
  std::map<std::string, mojo::Remote<blink::mojom::CacheStorage>>
      cache_storage_remote_map_;
  mojo::Remote<blink::mojom::CacheStorage> null_remote_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BackgroundFetchDataManager> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DATA_MANAGER_H_
