// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ANDROID_CDM_MEDIA_DRM_ORIGIN_ID_MANAGER_H_
#define CHROME_BROWSER_MEDIA_ANDROID_CDM_MEDIA_DRM_ORIGIN_ID_MANAGER_H_

#include <memory>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/unguessable_token.h"
#include "components/keyed_service/core/keyed_service.h"
#include "media/base/media_drm_storage.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class MediaDrmOriginIdManagerFactory;
class PrefRegistrySimple;
class PrefService;

// Implements a manager that preprovisions origin IDs used by MediaDrm so that
// an origin ID is available when attempting to play protected content in a
// partially offline environment (e.g. in flight entertainment). One
// MediaDrmOriginIdManager will be created for each PrefService the first time
// it is accessed. These objects need to be persistent so that they can
// pre-provision origin IDs in the background.
//
// These objects will be owned by MediaDrmOriginIdManagerFactory. They support
// KeyedService as the factory connects them to a Profile, and will be
// destroyed when the Profile goes away.
class MediaDrmOriginIdManager : public KeyedService {
 public:
  enum class GetOriginIdStatus {
    kSuccessWithPreProvisionedOriginId = 0,
    kSuccessWithNewlyProvisionedOriginId = 1,
    kFailure = 2,
  };

  using MediaDrmOriginId = media::MediaDrmStorage::MediaDrmOriginId;

  // |success| is true if an origin ID was obtained and |origin_id| is
  // not null, false otherwise.
  using ProvisionedOriginIdCB =
      base::OnceCallback<void(GetOriginIdStatus status,
                              const MediaDrmOriginId& origin_id)>;
  using ProvisioningResultCB = base::RepeatingCallback<MediaDrmOriginId()>;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Destructor must be public as it's used in std::unique_ptr<>.
  ~MediaDrmOriginIdManager() override;

  // Asynchronously preprovision origin IDs if necessary.
  void PreProvisionIfNecessary();

  // Asynchronously returns a preprovisioned origin ID using |callback|, if one
  // is available. If none are available, an un-provisioned origin ID is
  // returned.
  // TODO(crbug.com/917527): Return an empty origin ID once callers
  // can handle it.
  void GetOriginId(ProvisionedOriginIdCB callback);

  // When testing, use the provided |cb| instead of calling MediaDrm.
  void SetProvisioningResultCBForTesting(ProvisioningResultCB cb) {
    provisioning_result_cb_for_testing_ = cb;
  }

 private:
  class NetworkObserver;
  friend class MediaDrmOriginIdManagerFactory;

  // MediaDrmOriginIdManager should only be created by
  // MediaDrmOriginIdManagerFactory.
  explicit MediaDrmOriginIdManager(PrefService* pref_service);

  // Complete the pre-provisioning steps.
  void ResumePreProvisionIfNecessary(
      bool is_per_application_provisioning_supported);

  // Asynchronously call StartProvisioning() on a sequence using different
  // priorities, depending on |run_in_background|.
  void StartProvisioningAsync(bool run_in_background);

  // Called when provisioning of |origin_id| is done. The provisioning of
  // |origin_id| was successful if |origin_id| is not nullopt.
  void OriginIdProvisioned(const MediaDrmOriginId& origin_id);

  // Check if per application provisioning is supported or not. Uses
  // `is_per_application_provisioning_supported_`, and if not set, sets it.
  bool IsPerApplicationProvisioningSupported();

  // If called, record the current number of pre-provisioned origin IDs to UMA.
  void RecordCountOfPreprovisionedOriginIds();

  const raw_ptr<PrefService> pref_service_;

  // Callback to be used when the next origin ID is provisioned.
  base::queue<ProvisionedOriginIdCB> pending_provisioned_origin_id_cbs_;

  // True if this class is currently pre-provisioning origin IDs,
  // false otherwise.
  bool is_provisioning_ = false;

  // True if per-application provisioning is supported. If nullopt, then
  // support has not yet been determined.
  absl::optional<bool> is_per_application_provisioning_supported_;

  // When testing don't call MediaDrm to provision the origin ID, just call
  // this CB and use the value returned to indicate if provisioning succeeded or
  // failed so that tests can verify that the preference is used correctly.
  ProvisioningResultCB provisioning_result_cb_for_testing_;

  // When set, watch for network changes and call PreProvisionIfNecessary()
  // when connected to a network.
  std::unique_ptr<NetworkObserver> network_observer_;

  THREAD_CHECKER(thread_checker_);

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaDrmOriginIdManager> weak_factory_{this};
};

#endif  // CHROME_BROWSER_MEDIA_ANDROID_CDM_MEDIA_DRM_ORIGIN_ID_MANAGER_H_
