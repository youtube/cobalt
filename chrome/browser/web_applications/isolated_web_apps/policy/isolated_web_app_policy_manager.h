// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_POLICY_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_POLICY_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace web_app {

// This component is responsible for installing, uninstalling, updating etc.
// of the policy installed IWAs.
class IsolatedWebAppPolicyManager {
 public:
  enum class EphemeralAppInstallResult {
    kSuccess,
    kErrorNotEphemeralSession,
    kErrorCantCreateRootDirectory,
    kErrorUpdateManifestDownloadFailed,
    kErrorUpdateManifestParsingFailed,
    kErrorWebBundleUrlCantBeDetermined,
    kErrorCantCreateIwaDirectory,
    kErrorCantDownloadWebBundle,
    kErrorCantInstallFromWebBundle,
    kUnknown,
  };
  static constexpr char kEphemeralIwaRootDirectory[] = "EphemeralIWA";
  static constexpr char kMainSignedWebBundleFileName[] = "main.swbn";

  // This pure virtual class represents the IWA installation logic.
  // It is introduced primarily for testability reasons.
  class IwaInstallCommandWrapper {
   public:
    IwaInstallCommandWrapper() = default;
    IwaInstallCommandWrapper(const IwaInstallCommandWrapper&) = delete;
    IwaInstallCommandWrapper& operator=(const IwaInstallCommandWrapper&) =
        delete;
    virtual ~IwaInstallCommandWrapper() = default;
    virtual void Install(
        const IsolatedWebAppLocation& location,
        const IsolatedWebAppUrlInfo& url_info,
        WebAppCommandScheduler::InstallIsolatedWebAppCallback callback) = 0;
  };

  class IwaInstallCommandWrapperImpl : public IwaInstallCommandWrapper {
   public:
    explicit IwaInstallCommandWrapperImpl(web_app::WebAppProvider* provider);
    void Install(const IsolatedWebAppLocation& location,
                 const IsolatedWebAppUrlInfo& url_info,
                 WebAppCommandScheduler::InstallIsolatedWebAppCallback callback)
        override;
    ~IwaInstallCommandWrapperImpl() override = default;

   private:
    const raw_ptr<web_app::WebAppProvider> provider_;
  };

  IsolatedWebAppPolicyManager(
      const base::FilePath& context_dir,
      std::vector<IsolatedWebAppExternalInstallOptions>
          ephemeral_iwa_install_options,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<IwaInstallCommandWrapper> installer,
      base::OnceCallback<void(std::vector<EphemeralAppInstallResult>)>
          ephemeral_install_cb);
  ~IsolatedWebAppPolicyManager();

  // Triggers installing of the IWAs in MGS. There is no callback as so far we
  // don't care about the result of the installation: for MVP it is not critical
  // to have a complex retry mechanism for the session that would exist for just
  // several minutes.
  void InstallEphemeralApps();

  IsolatedWebAppPolicyManager(const IsolatedWebAppPolicyManager&) = delete;
  IsolatedWebAppPolicyManager& operator=(const IsolatedWebAppPolicyManager&) =
      delete;

  // Extracts the URL of the Web Bundle that corresponds to the latest version
  // of the app in the Update Manifest.
  static absl::optional<GURL> ExtractWebBundleURL(
      const base::Value& parsed_update_manifest);

 private:
  // Creating root directory where the ephemeral apps will be placed.
  void CreateIwaEphemeralRootDirectory();
  void OnIwaEphemeralRootDirectoryCreated(base::File::Error error);

  // Downloading of the update manifest of the current app.
  void DownloadUpdateManifest();
  void OnUpdateManifestDownloaded(
      std::unique_ptr<network::SimpleURLLoader> simple_loader,
      std::unique_ptr<std::string>);

  // Parsing of the update manifest from JSON string to Value tree.
  void ParseUpdateManifest(const std::string& manifest_content);
  void OnUpdateManifestParsed(absl::optional<base::Value> result,
                              const absl::optional<std::string>& error);

  // Create a new directory for the exact instance of the IWA.
  void CreateIwaDirectory();
  void OnIwaDirectoryCreated(const base::FilePath& iwa_dir,
                             base::File::Error error);

  // Downloading of the Signed Web Bundle.
  void DownloadWebBundle();
  void OnWebBundleDownloaded(
      std::unique_ptr<network::SimpleURLLoader> simple_loader,
      base::FilePath path);

  // Installing of the IWA using the downloaded Signed Web Bundle.
  void InstallIwa(base::FilePath path);
  void OnIwaInstalled(base::expected<InstallIsolatedWebAppCommandSuccess,
                                     InstallIsolatedWebAppCommandError> result);

  // Completely removes IWA directory.
  void WipeCurrentIwaDirectory();
  void OnCurrentIwaDirectoryWiped(bool wipe_result);

  void SetResultAndContinue(EphemeralAppInstallResult result);
  void SetResultForAllAndFinish(EphemeralAppInstallResult result);
  void ContinueWithTheNextApp();

  data_decoder::mojom::JsonParser* GetJsonParserPtr();

  // Isolated Web Apps for installation in ephemeral managed guest session.
  std::vector<IsolatedWebAppExternalInstallOptions>
      ephemeral_iwa_install_options_;
  std::vector<IsolatedWebAppExternalInstallOptions>::iterator current_app_;
  const base::FilePath installation_dir_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The result vector contains the installation result for each app.
  std::vector<EphemeralAppInstallResult> result_vector_;
  std::unique_ptr<IwaInstallCommandWrapper> installer_;
  base::OnceCallback<void(std::vector<EphemeralAppInstallResult>)>
      ephemeral_install_cb_;

  data_decoder::DataDecoder data_decoder_;
  // Dont use this variable directly. Use GetJsonParserPtr() instead.
  mojo::Remote<data_decoder::mojom::JsonParser> json_parser_;

  base::WeakPtrFactory<IsolatedWebAppPolicyManager> weak_factory_{this};
};

}  // namespace web_app
#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_POLICY_MANAGER_H_
