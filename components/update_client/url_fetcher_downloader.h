// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_URL_FETCHER_DOWNLOADER_H_
#define COMPONENTS_UPDATE_CLIENT_URL_FETCHER_DOWNLOADER_H_

#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/update_client/crx_downloader.h"
#include "components/update_client/update_client_errors.h"

#if defined(STARBOARD)
#include "components/update_client/cobalt_slot_management.h"
#endif

namespace update_client {

class NetworkFetcher;
class NetworkFetcherFactory;

// Implements a CRX downloader using a NetworkFetcher object.
class UrlFetcherDownloader : public CrxDownloader {
 public:
#if defined(STARBOARD)
  UrlFetcherDownloader(std::unique_ptr<CrxDownloader> successor,
                       scoped_refptr<Configurator> config);
#else
  UrlFetcherDownloader(
      std::unique_ptr<CrxDownloader> successor,
      scoped_refptr<NetworkFetcherFactory> network_fetcher_factory);
#endif
  ~UrlFetcherDownloader() override;

 private:
  // Overrides for CrxDownloader.
#if defined(IN_MEMORY_UPDATES)
  void DoStartDownload(const GURL& url, std::string* dst) override;
#else
  void DoStartDownload(const GURL& url) override;
#endif
#if defined(STARBOARD)
  void DoCancelDownload() override;
#endif

#if !defined(IN_MEMORY_UPDATES)
  void CreateDownloadDir();
#endif
#if defined(IN_MEMORY_UPDATES)
  // Does not take ownership of |dst|, which must refer to a valid string that
  // outlives this object.
  void StartURLFetch(const GURL& url, std::string* dst);
#else
  void StartURLFetch(const GURL& url);
#endif

#if defined(STARBOARD)
#if defined(IN_MEMORY_UPDATES)
  // With in-memory updates it's no longer necessary to select and confirm the
  // installation slot at download time, and it would likely be more clear to
  // move this responsibility to the unpack flow. That said, yavor@ requested
  // that we keep it here for now since there are a number of edge cases to
  // consider and data races to avoid in this space (e.g., racing updaters). To
  // limit the scope and risk of the in-memory updates changes we can leave it
  // here for now and continue passing the installation dir on to the unpack
  // flow, and consider changing this in a future refactoring.

  // Does not take ownership of |dst|, which must refer to a valid string that
  // outlives this object.
  void SelectSlot(const GURL& url, std::string* dst);

  // Does not take ownership of |dst|, which must refer to a valid string that
  // outlives this object.
  void ConfirmSlot(const GURL& url, std::string* dst);
#else  // defined(IN_MEMORY_UPDATES)
  void SelectSlot(const GURL& url);
  void ConfirmSlot(const GURL& url);
#endif  // defined(IN_MEMORY_UPDATES)
#endif  // defined(STARBOARD)
#if defined(IN_MEMORY_UPDATES)
  // Does not take ownership of |dst|, which must refer to a valid string that
  // outlives this object.
  void OnNetworkFetcherComplete(std::string* dst,
                                int net_error,
                                int64_t content_size);
#else
  void OnNetworkFetcherComplete(base::FilePath file_path,
                                int net_error,
                                int64_t content_size);
#endif
  void OnResponseStarted(const GURL& final_url,
                         int response_code,
                         int64_t content_length);
  void OnDownloadProgress(int64_t content_length);
  void ReportDownloadFailure(const GURL& url);
#if defined(STARBOARD)
  void ReportDownloadFailure(const GURL& url, CrxDownloaderError error);
#endif

  THREAD_CHECKER(thread_checker_);

  scoped_refptr<NetworkFetcherFactory> network_fetcher_factory_;
  std::unique_ptr<NetworkFetcher> network_fetcher_;

#if !defined(IN_MEMORY_UPDATES)
  // Contains a temporary download directory for the downloaded file.
  base::FilePath download_dir_;
#else
  // For legacy, file-system based updates the download_dir_ doubles as the
  // installation dir. But for in-memory updates this directory only serves as
  // the installation directory and should be named accordingly.
  base::FilePath installation_dir_;
#endif

  base::TimeTicks download_start_time_;

  GURL final_url_;
  int response_code_ = -1;
  int64_t total_bytes_ = -1;

#if defined(STARBOARD)
  CobaltSlotManagement cobalt_slot_management_;
  scoped_refptr<Configurator> config_;
  bool is_cancelled_ = false;
#endif

  DISALLOW_COPY_AND_ASSIGN(UrlFetcherDownloader);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_URL_FETCHER_DOWNLOADER_H_
