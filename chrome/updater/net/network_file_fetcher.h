// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_NET_NETWORK_FILE_FETCHER_H_
#define CHROME_UPDATER_NET_NETWORK_FILE_FETCHER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "components/update_client/network.h"
#include "mojo/public/cpp/system/data_pipe.h"

class GURL;

namespace updater {

// A customized fetcher that streams the download to a Mojo data pipe.
class NetworkStreamFetcher
    : public base::RefCountedThreadSafe<NetworkStreamFetcher> {
 public:
  NetworkStreamFetcher();
  NetworkStreamFetcher& operator=(const NetworkStreamFetcher&) = delete;
  NetworkStreamFetcher(const NetworkStreamFetcher&) = delete;

  base::OnceClosure Download(
      const GURL& url,
      mojo::ScopedDataPipeProducerHandle response_stream,
      update_client::NetworkFetcher::ResponseStartedCallback
          response_started_callback,
      update_client::NetworkFetcher::DownloadToFileCompleteCallback
          download_to_file_complete_callback);

 private:
  friend class base::RefCountedThreadSafe<NetworkStreamFetcher>;

  ~NetworkStreamFetcher();

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace updater

#endif  // CHROME_UPDATER_NET_NETWORK_FILE_FETCHER_H_
