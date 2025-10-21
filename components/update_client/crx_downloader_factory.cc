// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/crx_downloader_factory.h"

#include "build/build_config.h"
#if BUILDFLAG(IS_WIN)
#include "components/update_client/background_downloader_win.h"
#endif
#include "components/update_client/crx_downloader.h"
#include "components/update_client/network.h"
#include "components/update_client/url_fetcher_downloader.h"

#if BUILDFLAG(IS_STARBOARD)
#include "components/update_client/configurator.h"
#endif

namespace update_client {
namespace {

#if BUILDFLAG(IS_STARBOARD)
class CrxDownloaderFactoryCobalt : public CrxDownloaderFactory {
 public:
  explicit CrxDownloaderFactoryCobalt(
      scoped_refptr<NetworkFetcherFactory> network_fetcher_factory)
      : network_fetcher_factory_(std::move(network_fetcher_factory)) {}

  // Overrides for CrxDownloaderFactory.
  scoped_refptr<CrxDownloader> MakeCrxDownloader(
      scoped_refptr<Configurator> config) const override;

 private:
  ~CrxDownloaderFactoryCobalt() override = default;

  scoped_refptr<NetworkFetcherFactory> network_fetcher_factory_;
};

scoped_refptr<CrxDownloader> CrxDownloaderFactoryCobalt::MakeCrxDownloader(
    scoped_refptr<Configurator> config) const {
  scoped_refptr<CrxDownloader> url_fetcher_downloader =
      base::MakeRefCounted<UrlFetcherDownloader>(nullptr, std::move(config));

  return url_fetcher_downloader;
}

#else // BUILDFLAG(IS_STARBOARD)
class CrxDownloaderFactoryChromium : public CrxDownloaderFactory {
 public:
  explicit CrxDownloaderFactoryChromium(
      scoped_refptr<NetworkFetcherFactory> network_fetcher_factory)
      : network_fetcher_factory_(network_fetcher_factory) {}

  // Overrides for CrxDownloaderFactory.
  scoped_refptr<CrxDownloader> MakeCrxDownloader(
      bool background_download_enabled) const override;

 private:
  ~CrxDownloaderFactoryChromium() override = default;

  scoped_refptr<NetworkFetcherFactory> network_fetcher_factory_;
};

scoped_refptr<CrxDownloader> CrxDownloaderFactoryChromium::MakeCrxDownloader(
    bool background_download_enabled) const {
  scoped_refptr<CrxDownloader> url_fetcher_downloader =
      base::MakeRefCounted<UrlFetcherDownloader>(nullptr,
                                                 network_fetcher_factory_);
#if BUILDFLAG(IS_WIN)
  // If background downloads are allowed, then apply the BITS service
  // background downloader first.
  if (background_download_enabled) {
    return base::MakeRefCounted<BackgroundDownloader>(url_fetcher_downloader);
  }
#endif // BUILDFLAG(IS_STARBOARD)

  return url_fetcher_downloader;
}
#endif

}  // namespace

scoped_refptr<CrxDownloaderFactory> MakeCrxDownloaderFactory(
    scoped_refptr<NetworkFetcherFactory> network_fetcher_factory) {
#if BUILDFLAG(IS_STARBOARD)
  return base::MakeRefCounted<CrxDownloaderFactoryCobalt>(
      std::move(network_fetcher_factory));
#else
  return base::MakeRefCounted<CrxDownloaderFactoryChromium>(
      network_fetcher_factory);
#endif
}

}  // namespace update_client
