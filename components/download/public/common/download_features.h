// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_FEATURES_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/download/public/common/download_export.h"

namespace download {
namespace features {

// Whether offline content provider should be used for the downloads UI..
COMPONENTS_DOWNLOAD_EXPORT BASE_DECLARE_FEATURE(
    kUseDownloadOfflineContentProvider);

// Whether a download can be handled by parallel jobs.
COMPONENTS_DOWNLOAD_EXPORT BASE_DECLARE_FEATURE(kParallelDownloading);

#if BUILDFLAG(IS_ANDROID)
// Whether download expiration date will be refreshed on resumption.
COMPONENTS_DOWNLOAD_EXPORT BASE_DECLARE_FEATURE(kRefreshExpirationDate);

// Whether to enable smart suggestion for large downloads
COMPONENTS_DOWNLOAD_EXPORT BASE_DECLARE_FEATURE(
    kSmartSuggestionForLargeDownloads);
#endif

// Whether in-progress download manager will be used to initialize download
// service.
COMPONENTS_DOWNLOAD_EXPORT BASE_DECLARE_FEATURE(
    kUseInProgressDownloadManagerForDownloadService);

// Whether download resumption is allowed when there are no strong validators.
COMPONENTS_DOWNLOAD_EXPORT BASE_DECLARE_FEATURE(
    kAllowDownloadResumptionWithoutStrongValidators);

// Whether parallel download is used for HTTP2 connections.
COMPONENTS_DOWNLOAD_EXPORT BASE_DECLARE_FEATURE(kUseParallelRequestsForHTTP2);

// Whether parallel download is used for QUIC connections.
COMPONENTS_DOWNLOAD_EXPORT BASE_DECLARE_FEATURE(kUseParallelRequestsForQUIC);

// Whether to delete expired download.
COMPONENTS_DOWNLOAD_EXPORT BASE_DECLARE_FEATURE(kDeleteExpiredDownloads);

// Whether to delete downloads that are overwritten by others.
COMPONENTS_DOWNLOAD_EXPORT BASE_DECLARE_FEATURE(kDeleteOverwrittenDownloads);

// Whether to allow changing the size of file buffer.
COMPONENTS_DOWNLOAD_EXPORT BASE_DECLARE_FEATURE(kAllowFileBufferSizeControl);

// Arbitrary range request support for download system.
COMPONENTS_DOWNLOAD_EXPORT BASE_DECLARE_FEATURE(kDownloadRange);
}  // namespace features

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_FEATURES_H_
