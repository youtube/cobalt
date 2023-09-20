// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_TEST_CONFIGURATOR_H_
#define COMPONENTS_UPDATE_CLIENT_TEST_CONFIGURATOR_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/update_client/configurator.h"
#include "url/gurl.h"

#if defined(STARBOARD)
#include "cobalt/network/network_module.h"
#include "components/update_client/net/network_cobalt.h"
#else
#include "services/network/test/test_url_loader_factory.h"
#endif

class PrefService;

#if !defined(STARBOARD)
namespace network {
class SharedURLLoaderFactory;
}  // namespace network
#endif

namespace update_client {

class ActivityDataService;
class ProtocolHandlerFactory;
#if defined(STARBOARD)
class NetworkFetcherCobaltFactory;
class PatchCobaltFactory;
class UnzipCobaltFactory;
#else
class NetworkFetcherFactory;
class PatchChromiumFactory;
class UnzipChromiumFactory;
#endif

#define POST_INTERCEPT_SCHEME "https"
#define POST_INTERCEPT_HOSTNAME "localhost2"
#define POST_INTERCEPT_PATH "/update2"

// component 1 has extension id "jebgalgnebhfojomionfpkfelancnnkf", and
// the RSA public key the following hash:
const uint8_t jebg_hash[] = {0x94, 0x16, 0x0b, 0x6d, 0x41, 0x75, 0xe9, 0xec,
                             0x8e, 0xd5, 0xfa, 0x54, 0xb0, 0xd2, 0xdd, 0xa5,
                             0x6e, 0x05, 0x6b, 0xe8, 0x73, 0x47, 0xf6, 0xc4,
                             0x11, 0x9f, 0xbc, 0xb3, 0x09, 0xb3, 0x5b, 0x40};
// component 1 public key (base64 encoded):
const char jebg_public_key[] =
    "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQC68bW8i/RzSaeXOcNLuBw0SP9+1bdo5ysLqH"
    "qfLqZs6XyJWEyL0U6f1axPR6LwViku21kgdc6PI524eb8Cr+a/iXGgZ8SdvZTcfQ/g/ukwlblF"
    "mtqYfDoVpz03U8rDQ9b6DxeJBF4r48TNlFORggrAiNR26qbf1i178Au12AzWtwIDAQAB";
// component 2 has extension id "abagagagagagagagagagagagagagagag", and
// the RSA public key the following hash:
const uint8_t abag_hash[] = {0x01, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
                             0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
                             0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
                             0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x01};
// component 3 has extension id "ihfokbkgjpifnbbojhneepfflplebdkc", and
// the RSA public key the following hash:
const uint8_t ihfo_hash[] = {0x87, 0x5e, 0xa1, 0xa6, 0x9f, 0x85, 0xd1, 0x1e,
                             0x97, 0xd4, 0x4f, 0x55, 0xbf, 0xb4, 0x13, 0xa2,
                             0xe7, 0xc5, 0xc8, 0xf5, 0x60, 0x19, 0x78, 0x1b,
                             0x6d, 0xe9, 0x4c, 0xeb, 0x96, 0x05, 0x42, 0x17};

// runaction_test_win.crx and its payload id: gjpmebpgbhcamgdgjcmnjfhggjpgcimm
const uint8_t gjpm_hash[] = {0x69, 0xfc, 0x41, 0xf6, 0x17, 0x20, 0xc6, 0x36,
                             0x92, 0xcd, 0x95, 0x76, 0x69, 0xf6, 0x28, 0xcc,
                             0xbe, 0x98, 0x4b, 0x93, 0x17, 0xd6, 0x9c, 0xb3,
                             0x64, 0x0c, 0x0d, 0x25, 0x61, 0xc5, 0x80, 0x1d};

class TestConfigurator : public Configurator {
 public:
  TestConfigurator();

  // Overrides for Configurator.
  int InitialDelay() const override;
  int NextCheckDelay() const override;
  int OnDemandDelay() const override;
  int UpdateDelay() const override;
  std::vector<GURL> UpdateUrl() const override;
  std::vector<GURL> PingUrl() const override;
  std::string GetProdId() const override;
  base::Version GetBrowserVersion() const override;
  std::string GetChannel() const override;
  std::string GetBrand() const override;
  std::string GetLang() const override;
  std::string GetOSLongName() const override;
  base::flat_map<std::string, std::string> ExtraRequestParams() const override;
  std::string GetDownloadPreference() const override;
  scoped_refptr<NetworkFetcherFactory> GetNetworkFetcherFactory() override;
  scoped_refptr<UnzipperFactory> GetUnzipperFactory() override;
  scoped_refptr<PatcherFactory> GetPatcherFactory() override;
  bool EnabledDeltas() const override;
  bool EnabledComponentUpdates() const override;
  bool EnabledBackgroundDownloader() const override;
  bool EnabledCupSigning() const override;
  PrefService* GetPrefService() const override;
  ActivityDataService* GetActivityDataService() const override;
  bool IsPerUserInstall() const override;
  std::vector<uint8_t> GetRunActionKeyHash() const override;
  std::string GetAppGuid() const override;
  std::unique_ptr<ProtocolHandlerFactory> GetProtocolHandlerFactory()
      const override;
  RecoveryCRXElevator GetRecoveryCRXElevator() const override;

  void SetBrand(const std::string& brand);
  void SetOnDemandTime(int seconds);
  void SetInitialDelay(int seconds);
  void SetDownloadPreference(const std::string& download_preference);
  void SetEnabledCupSigning(bool use_cup_signing);
  void SetEnabledComponentUpdates(bool enabled_component_updates);
  void SetUpdateCheckUrl(const GURL& url);
  void SetPingUrl(const GURL& url);
  void SetAppGuid(const std::string& app_guid);

#if defined(STARBOARD)
  // TODO: add unit tests for updater channels, status, and compressed updates.
  void SetChannel(const std::string& channel) override {}
  void CompareAndSwapChannelChanged(int old_value, int new_value) override {}
  std::string GetUpdaterStatus() const override { return ""; }
  void SetUpdaterStatus(const std::string& status) override {}

  std::string GetPreviousUpdaterStatus() const override { return ""; }
  void SetPreviousUpdaterStatus(const std::string& status) override {}

  void SetMinFreeSpaceBytes(uint64_t bytes) override {}

  uint64_t GetMinFreeSpaceBytes() override { return 0; }

  bool GetUseCompressedUpdates() const override { return false; }
  void SetUseCompressedUpdates(bool use_compressed_updates) override {}
#else
  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }
#endif

 private:
  friend class base::RefCountedThreadSafe<TestConfigurator>;
  ~TestConfigurator() override;

  class TestPatchService;

  std::string brand_;
  int initial_time_;
  int ondemand_time_;
  std::string download_preference_;
  bool enabled_cup_signing_;
  bool enabled_component_updates_;
  GURL update_check_url_;
  GURL ping_url_;
  std::string app_guid_;

#if defined(STARBOARD)
  scoped_refptr<update_client::UnzipCobaltFactory> unzip_factory_;
  scoped_refptr<update_client::PatchCobaltFactory> patch_factory_;

  scoped_refptr<NetworkFetcherCobaltFactory> network_fetcher_factory_;
  std::unique_ptr<cobalt::network::NetworkModule> network_module_;
#else
  scoped_refptr<update_client::UnzipChromiumFactory> unzip_factory_;
  scoped_refptr<update_client::PatchChromiumFactory> patch_factory_;

  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<NetworkFetcherFactory> network_fetcher_factory_;
#endif

  DISALLOW_COPY_AND_ASSIGN(TestConfigurator);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_TEST_CONFIGURATOR_H_
