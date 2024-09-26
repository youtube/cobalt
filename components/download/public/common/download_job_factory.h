// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_JOB_FACTORY_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_JOB_FACTORY_H_

#include <memory>

#include "base/functional/callback.h"
#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_job.h"
#include "components/download/public/common/url_loader_factory_provider.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

namespace download {
class DownloadItem;

// Factory class to create different kinds of DownloadJob.
class COMPONENTS_DOWNLOAD_EXPORT DownloadJobFactory {
 public:
  // A callback that can be called to bind a device.mojom.WakeLockProvider
  // receiver.
  using WakeLockProviderBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<device::mojom::WakeLockProvider>)>;

  DownloadJobFactory(const DownloadJobFactory&) = delete;
  DownloadJobFactory& operator=(const DownloadJobFactory&) = delete;

  static std::unique_ptr<DownloadJob> CreateJob(
      DownloadItem* download_item,
      DownloadJob::CancelRequestCallback cancel_request_callback,
      const DownloadCreateInfo& create_info,
      bool is_save_package_download,
      URLLoaderFactoryProvider::URLLoaderFactoryProviderPtr
          url_loader_factory_provider,
      WakeLockProviderBinder wake_lock_provider_binder);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_JOB_FACTORY_H_
