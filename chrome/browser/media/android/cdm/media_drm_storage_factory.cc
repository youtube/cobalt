// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/cdm/media_drm_storage_factory.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/media/android/cdm/media_drm_origin_id_manager.h"
#include "chrome/browser/media/android/cdm/media_drm_origin_id_manager_factory.h"
#include "chrome/browser/media/android/cdm/per_device_provisioning_permission.h"
#include "chrome/browser/profiles/profile.h"
#include "components/cdm/browser/media_drm_storage_impl.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/media_switches.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

using MediaDrmOriginId = media::MediaDrmStorage::MediaDrmOriginId;
using GetOriginIdStatus = MediaDrmOriginIdManager::GetOriginIdStatus;
using OriginIdReadyCB =
    base::OnceCallback<void(bool success, const MediaDrmOriginId& origin_id)>;

// These values are reported to UMA. Entries should not be renumbered and
// numeric values should never be reused.
enum class GetOriginIdResult {
  kSuccessWithPreProvisionedOriginId = 0,
  kSuccessWithNewlyProvisionedOriginId = 1,
  kSuccessWithUnprovisionedOriginId = 2,
  kFailureOnPerAppProvisioningDevice = 3,
  kFailureOnNonPerAppProvisioningDevice = 4,
  kFailureWithNoFactory = 5,
  kMaxValue = kFailureWithNoFactory,
};

GetOriginIdResult ConvertGetOriginIdStatusToResult(GetOriginIdStatus status) {
  switch (status) {
    case GetOriginIdStatus::kSuccessWithPreProvisionedOriginId:
      return GetOriginIdResult::kSuccessWithPreProvisionedOriginId;
    case GetOriginIdStatus::kSuccessWithNewlyProvisionedOriginId:
      return GetOriginIdResult::kSuccessWithNewlyProvisionedOriginId;
    case GetOriginIdStatus::kFailure:
      break;
  }

  return media::MediaDrmBridge::IsPerApplicationProvisioningSupported()
             ? GetOriginIdResult::kFailureOnPerAppProvisioningDevice
             : GetOriginIdResult::kFailureOnNonPerAppProvisioningDevice;
}

// Update UMA with |result|.
void ReportResultToUma(GetOriginIdResult result) {
  base::UmaHistogramEnumeration("Media.EME.MediaDrm.GetOriginIdResult", result);
}

// Update UMA with |status|, and then pass |origin_id| to |callback|.
void ReportStatusToUmaAndNotifyCaller(OriginIdReadyCB callback,
                                      GetOriginIdStatus status,
                                      const MediaDrmOriginId& origin_id) {
  ReportResultToUma(ConvertGetOriginIdStatusToResult(status));
  std::move(callback).Run(status != GetOriginIdStatus::kFailure, origin_id);
}

void CreateOriginIdWithMediaDrmOriginIdManager(Profile* profile,
                                               OriginIdReadyCB callback) {
  auto* origin_id_manager =
      MediaDrmOriginIdManagerFactory::GetForProfile(profile);
  if (!origin_id_manager) {
    ReportResultToUma(GetOriginIdResult::kFailureWithNoFactory);
    std::move(callback).Run(false, absl::nullopt);
    return;
  }

  origin_id_manager->GetOriginId(
      base::BindOnce(&ReportStatusToUmaAndNotifyCaller, std::move(callback)));
}

void CreateOriginId(OriginIdReadyCB callback) {
  auto origin_id = base::UnguessableToken::Create();
  DVLOG(2) << __func__ << ": origin_id = " << origin_id;

  ReportResultToUma(GetOriginIdResult::kSuccessWithUnprovisionedOriginId);
  std::move(callback).Run(true, origin_id);
}

void AllowEmptyOriginId(content::RenderFrameHost* render_frame_host,
                        base::OnceCallback<void(bool)> callback) {
  if (media::MediaDrmBridge::IsPerApplicationProvisioningSupported()) {
    // If per-application provisioning is supported by the device, use of the
    // empty origin ID won't work so don't allow it.
    std::move(callback).Run(false);
    return;
  }

  // Check if the user will allow use of the per-device identifier.
  RequestPerDeviceProvisioningPermission(render_frame_host,
                                         std::move(callback));
}

}  // namespace

void CreateMediaDrmStorage(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<media::mojom::MediaDrmStorage> receiver) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(render_frame_host);

  content::BrowserContext* browser_context =
      render_frame_host->GetBrowserContext();
  DCHECK(browser_context) << "BrowserContext not available.";

  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile) << "Profile not available.";

  PrefService* pref_service = profile->GetPrefs();
  DCHECK(pref_service) << "PrefService not available.";

  if (render_frame_host->GetLastCommittedOrigin().opaque()) {
    DVLOG(1) << __func__ << ": Unique origin.";
    return;
  }

  // Only use MediaDrmOriginIdManager's preprovisioned origin IDs when feature
  // kMediaDrmPreprovisioning is enabled.
  auto get_origin_id_cb =
      base::FeatureList::IsEnabled(media::kMediaDrmPreprovisioning)
          ? base::BindRepeating(&CreateOriginIdWithMediaDrmOriginIdManager,
                                profile)
          : base::BindRepeating(&CreateOriginId);

  // The object will be deleted on connection error, or when the frame navigates
  // away. See DocumentService for details.
  new cdm::MediaDrmStorageImpl(
      *render_frame_host, pref_service, get_origin_id_cb,
      base::BindRepeating(&AllowEmptyOriginId, render_frame_host),
      std::move(receiver));
}
