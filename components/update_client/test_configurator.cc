// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/test_configurator.h"
#include "build/build_config.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/prefs/pref_service.h"
#include "components/services/patch/in_process_file_patcher.h"
#include "components/services/unzip/in_process_unzipper.h"
#include "components/update_client/activity_data_service.h"
#include "components/update_client/crx_cache.h"
#include "components/update_client/crx_downloader_factory.h"
#include "components/update_client/net/network_chromium.h"  // nogncheck
#include "components/update_client/patch/patch_impl.h"
#include "components/update_client/patcher.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/test_activity_data_service.h"
#include "components/update_client/unzip/unzip_impl.h"  // nogncheck
#include "components/update_client/unzipper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_STARBOARD) && defined(IN_MEMORY_UPDATES)
#include "base/memory/raw_ptr.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/zlib/google/zip.h"  // nogncheck
#endif

namespace update_client {

namespace {

#if BUILDFLAG(IS_STARBOARD) && defined(IN_MEMORY_UPDATES)
class TestUnzipper : public Unzipper {
 public:
  TestUnzipper() = default;

  void Unzip(const base::FilePath& zip_path,
             const base::FilePath& output_path,
             UnzipCompleteCallback callback) override {
    std::move(callback).Run(zip::Unzip(zip_path, output_path));
  }

  void Unzip(const std::string& zip_str,
             const base::FilePath& output_path,
             UnzipCompleteCallback callback) override {
    std::move(callback).Run(zip::Unzip(zip_str, output_path));
  }

  base::OnceClosure DecodeXz(const base::FilePath& xz_file,
                             const base::FilePath& destination,
                             UnzipCompleteCallback callback) override {
    return base::DoNothing();
  }
};

class TestNetworkFetcher : public NetworkFetcher {
 public:
  explicit TestNetworkFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_network_factory)
      : shared_url_network_factory_(shared_url_network_factory) {}

  ~TestNetworkFetcher() override = default;

  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      PostRequestCompleteCallback post_request_complete_callback) override {
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = url;
    resource_request->method = "POST";
    resource_request->load_flags = net::LOAD_DISABLE_CACHE;
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    for (const auto& [name, value] : post_additional_headers) {
      resource_request->headers.SetHeader(name, value);
    }
    simple_url_loader_ = network::SimpleURLLoader::Create(
        std::move(resource_request),
        net::DefineNetworkTrafficAnnotation("test", "test"));
    simple_url_loader_->AttachStringForUpload(post_data, content_type);
    simple_url_loader_->SetOnResponseStartedCallback(base::BindOnce(
        [](ResponseStartedCallback response_started_callback,
           const GURL& final_url,
           const network::mojom::URLResponseHead& response_head) {
          response_started_callback.Run(
              response_head.headers ? response_head.headers->response_code()
                                    : -1,
              response_head.content_length);
        },
        response_started_callback));
    simple_url_loader_->SetOnDownloadProgressCallback(base::BindRepeating(
        [](ProgressCallback progress_callback, uint64_t current) {
          progress_callback.Run(static_cast<int64_t>(current));
        },
        progress_callback));
    simple_url_loader_->DownloadToString(
        shared_url_network_factory_.get(),
        base::BindOnce(
            [](TestNetworkFetcher* fetcher,
               PostRequestCompleteCallback post_request_complete_callback,
               std::optional<std::string> response_body) {
              std::string etag, cup_proof, cookie;
              int64_t retry_after = -1;
              if (fetcher->simple_url_loader_->ResponseInfo() &&
                  fetcher->simple_url_loader_->ResponseInfo()->headers) {
                auto* headers = fetcher->simple_url_loader_->ResponseInfo()->headers.get();
                headers->EnumerateHeader(nullptr, kHeaderEtag, &etag);
                headers->EnumerateHeader(nullptr, kHeaderXCupServerProof, &cup_proof);
                headers->EnumerateHeader(nullptr, kHeaderCookie, &cookie);
                retry_after = headers->GetInt64HeaderValue(kHeaderXRetryAfter);
              }
              std::move(post_request_complete_callback)
                  .Run(std::move(response_body),
                       fetcher->simple_url_loader_->NetError(), etag, cup_proof,
                       cookie, retry_after);
            },
            base::Unretained(this),
            std::move(post_request_complete_callback)),
        1024 * 1024);
  }

  void DownloadToString(
      const GURL& url,
      std::string* dst,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      DownloadToStringCompleteCallback download_to_string_complete_callback) override {
    dst_str_ = dst;
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = url;
    resource_request->method = "GET";
    resource_request->load_flags = net::LOAD_DISABLE_CACHE;
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    simple_url_loader_ = network::SimpleURLLoader::Create(
        std::move(resource_request),
        net::DefineNetworkTrafficAnnotation("test", "test"));
    simple_url_loader_->SetOnResponseStartedCallback(base::BindOnce(
        [](ResponseStartedCallback response_started_callback,
           const GURL& final_url,
           const network::mojom::URLResponseHead& response_head) {
          response_started_callback.Run(
              response_head.headers ? response_head.headers->response_code()
                                    : -1,
              response_head.content_length);
        },
        response_started_callback));
    simple_url_loader_->SetOnDownloadProgressCallback(base::BindRepeating(
        [](ProgressCallback progress_callback, uint64_t current) {
          progress_callback.Run(static_cast<int64_t>(current));
        },
        progress_callback));
    simple_url_loader_->DownloadToString(
        shared_url_network_factory_.get(),
        base::BindOnce(
            [](TestNetworkFetcher* fetcher,
               DownloadToStringCompleteCallback download_to_string_complete_callback,
               std::unique_ptr<std::string> response_body) {
              if (response_body) {
                *fetcher->dst_str_ = std::move(*response_body);
              }
              std::move(download_to_string_complete_callback)
                  .Run(fetcher->dst_str_, fetcher->simple_url_loader_->NetError(),
                       fetcher->simple_url_loader_->GetContentSize());
            },
            base::Unretained(this),
            std::move(download_to_string_complete_callback)),
        network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
  }

  void Cancel() override {
    simple_url_loader_.reset();
  }

 private:
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_network_factory_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  base::raw_ptr<std::string> dst_str_;
};

class TestNetworkFetcherFactory : public NetworkFetcherFactory {
 public:
  explicit TestNetworkFetcherFactory(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_network_factory)
      : shared_url_network_factory_(shared_url_network_factory) {}

  std::unique_ptr<NetworkFetcher> Create() const override {
    return std::make_unique<TestNetworkFetcher>(shared_url_network_factory_);
  }

 protected:
  ~TestNetworkFetcherFactory() override = default;

 private:
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_network_factory_;
};
#endif

std::vector<GURL> MakeDefaultUrls() {
  std::vector<GURL> urls;
  urls.push_back(GURL(POST_INTERCEPT_SCHEME
                      "://" POST_INTERCEPT_HOSTNAME POST_INTERCEPT_PATH));
  return urls;
}

}  // namespace

#if BUILDFLAG(IS_STARBOARD) && defined(IN_MEMORY_UPDATES)
std::unique_ptr<Unzipper> TestUnzipperFactory::Create() const {
  return std::make_unique<TestUnzipper>();
}
#endif

TestConfigurator::TestConfigurator(PrefService* pref_service)
    : enabled_cup_signing_(false),
      pref_service_(pref_service),
#if BUILDFLAG(IS_STARBOARD) && defined(IN_MEMORY_UPDATES)
      unzip_factory_(base::MakeRefCounted<TestUnzipperFactory>()),
#else
      unzip_factory_(base::MakeRefCounted<update_client::UnzipChromiumFactory>(
          base::BindRepeating(&unzip::LaunchInProcessUnzipper))),
#endif
      patch_factory_(base::MakeRefCounted<update_client::PatchChromiumFactory>(
          base::BindRepeating(&patch::LaunchInProcessFilePatcher))),
      test_shared_loader_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)),
#if BUILDFLAG(IS_STARBOARD) && defined(IN_MEMORY_UPDATES)
      network_fetcher_factory_(
          base::MakeRefCounted<TestNetworkFetcherFactory>(
              test_shared_loader_factory_)),
#else
      network_fetcher_factory_(
          base::MakeRefCounted<NetworkFetcherChromiumFactory>(
              test_shared_loader_factory_,
              base::BindRepeating([](const GURL& url) { return false; }))),
#endif
      updater_state_provider_(base::BindRepeating(
          [](bool /*is_machine*/) { return UpdaterStateAttributes(); })),
      is_network_connection_metered_(false) {
  std::ignore = crx_cache_root_temp_dir_.CreateUniqueTempDir();
  crx_cache_ =
      base::MakeRefCounted<CrxCache>(crx_cache_root_temp_dir_.GetPath().Append(
          FILE_PATH_LITERAL("crx_cache")));
  auto activity = std::make_unique<TestActivityDataService>();
  activity_data_service_ = activity.get();
  persisted_data_ = CreatePersistedData(
      base::BindRepeating([](PrefService* pref) { return pref; }, pref_service),
      std::move(activity));
}

TestConfigurator::~TestConfigurator() = default;

base::TimeDelta TestConfigurator::InitialDelay() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return initial_time_;
}

base::TimeDelta TestConfigurator::NextCheckDelay() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Seconds(1);
}

base::TimeDelta TestConfigurator::OnDemandDelay() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ondemand_time_;
}

base::TimeDelta TestConfigurator::UpdateDelay() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Seconds(1);
}

std::vector<GURL> TestConfigurator::UpdateUrl() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!update_check_urls_.empty()) {
    return update_check_urls_;
  }

  return MakeDefaultUrls();
}

std::vector<GURL> TestConfigurator::PingUrl() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!ping_url_.is_empty()) {
    return std::vector<GURL>(1, ping_url_);
  }
  return UpdateUrl();
}

std::string TestConfigurator::GetProdId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return "fake_prodid";
}

base::Version TestConfigurator::GetBrowserVersion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Needs to be larger than the required version in tested component manifests.
  return base::Version("30.0");
}

std::string TestConfigurator::GetChannel() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return "fake_channel_string";
}

std::string TestConfigurator::GetLang() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return "fake_lang";
}

std::string TestConfigurator::GetOSLongName() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return "Fake Operating System";
}

base::flat_map<std::string, std::string> TestConfigurator::ExtraRequestParams()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return {{"extra", "foo"}};
}

std::string TestConfigurator::GetDownloadPreference() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return download_preference_;
}

scoped_refptr<NetworkFetcherFactory>
TestConfigurator::GetNetworkFetcherFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return network_fetcher_factory_;
}

scoped_refptr<CrxDownloaderFactory>
TestConfigurator::GetCrxDownloaderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return crx_downloader_factory_;
}

scoped_refptr<UnzipperFactory> TestConfigurator::GetUnzipperFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return unzip_factory_;
}

scoped_refptr<PatcherFactory> TestConfigurator::GetPatcherFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return patch_factory_;
}

bool TestConfigurator::EnabledBackgroundDownloader() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return false;
}

bool TestConfigurator::EnabledCupSigning() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return enabled_cup_signing_;
}

PrefService* TestConfigurator::GetPrefService() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_;
}

TestActivityDataService* TestConfigurator::GetActivityDataService() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return activity_data_service_;
}

PersistedData* TestConfigurator::GetPersistedData() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return persisted_data_.get();
}

bool TestConfigurator::IsPerUserInstall() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

std::unique_ptr<ProtocolHandlerFactory>
TestConfigurator::GetProtocolHandlerFactory() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<ProtocolHandlerFactoryJSON>();
}

std::optional<bool> TestConfigurator::IsMachineExternallyManaged() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_machine_externally_managed_;
}

UpdaterStateProvider TestConfigurator::GetUpdaterStateProvider() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return updater_state_provider_;
}

scoped_refptr<CrxCache> TestConfigurator::GetCrxCache() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return crx_cache_;
}

bool TestConfigurator::IsConnectionMetered() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_network_connection_metered_;
}

void TestConfigurator::SetOnDemandTime(base::TimeDelta time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ondemand_time_ = time;
}

void TestConfigurator::SetInitialDelay(base::TimeDelta delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  initial_time_ = delay;
}

void TestConfigurator::SetEnabledCupSigning(bool enabled_cup_signing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  enabled_cup_signing_ = enabled_cup_signing;
}

void TestConfigurator::SetDownloadPreference(
    const std::string& download_preference) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  download_preference_ = download_preference;
}

void TestConfigurator::SetUpdateCheckUrl(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  update_check_urls_ = {url};
}

void TestConfigurator::SetUpdateCheckUrls(const std::vector<GURL>& urls) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  update_check_urls_ = urls;
}

void TestConfigurator::SetPingUrl(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ping_url_ = url;
}

void TestConfigurator::SetCrxDownloaderFactory(
    scoped_refptr<CrxDownloaderFactory> crx_downloader_factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  crx_downloader_factory_ = crx_downloader_factory;
}

void TestConfigurator::SetIsMachineExternallyManaged(
    std::optional<bool> is_machine_externally_managed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_machine_externally_managed_ = is_machine_externally_managed;
}

void TestConfigurator::SetIsNetworkConnectionMetered(
    bool is_network_connection_metered) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_network_connection_metered_ = is_network_connection_metered;
}

void TestConfigurator::SetUpdaterStateProvider(
    UpdaterStateProvider update_state_provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  updater_state_provider_ = update_state_provider;
}

}  // namespace update_client
