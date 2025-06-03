// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_preloaded_verified_app_command.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_tree.h"
#include "base/functional/bind.h"
#include "base/strings/to_string.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/url_util.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest_manager.mojom.h"
#include "url/url_constants.h"

namespace web_app {

namespace {

// TODO(crbug.com/1457430): Find a better way to do Lacros testing so that we
// don't have to pass localhost into the allowlist. Allowlisted host must be
// from a Google server.
constexpr auto kHostAllowlist = base::MakeFixedFlatSet<base::StringPiece>(
    {"googleusercontent.com", "gstatic.com", "youtube.com",
     "127.0.0.1" /*FOR TESTING*/});

bool HasRequiredManifestFields(const blink::mojom::ManifestPtr& manifest) {
  if (manifest->start_url.is_empty()) {
    return false;
  }

  if (!manifest->short_name.has_value() && !manifest->name.has_value()) {
    return false;
  }

  return true;
}

}  // namespace

InstallPreloadedVerifiedAppCommand::InstallPreloadedVerifiedAppCommand(
    webapps::WebappInstallSource install_source,
    GURL document_url,
    GURL manifest_url,
    std::string manifest_contents,
    webapps::AppId expected_id,
    OnceInstallCallback callback)
    : WebAppCommandTemplate<SharedWebContentsLock>(
          "InstallPreloadedVerifiedAppCommand"),
      install_source_(install_source),
      document_url_(std::move(document_url)),
      manifest_url_(std::move(manifest_url)),
      manifest_contents_(std::move(manifest_contents)),
      expected_id_(std::move(expected_id)),
      install_callback_(std::move(callback)),
      web_contents_lock_description_(
          std::make_unique<SharedWebContentsLockDescription>()) {}

InstallPreloadedVerifiedAppCommand::~InstallPreloadedVerifiedAppCommand() =
    default;

const LockDescription& InstallPreloadedVerifiedAppCommand::lock_description()
    const {
  DCHECK(web_contents_lock_description_ || app_lock_description_);

  if (app_lock_description_) {
    return *app_lock_description_;
  }

  return *web_contents_lock_description_;
}

void InstallPreloadedVerifiedAppCommand::StartWithLock(
    std::unique_ptr<SharedWebContentsLock> lock) {
  web_contents_lock_ = std::move(lock);

  url_loader_ = web_contents_lock_->web_contents_manager().CreateUrlLoader();
  data_retriever_ =
      web_contents_lock_->web_contents_manager().CreateDataRetriever();
  url_loader_->LoadUrl(
      GURL(url::kAboutBlankURL), &web_contents_lock_->shared_web_contents(),
      WebAppUrlLoader::UrlComparison::kExact,
      base::BindOnce(&InstallPreloadedVerifiedAppCommand::OnAboutBlankLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

base::Value InstallPreloadedVerifiedAppCommand::ToDebugValue() const {
  base::Value::Dict debug_value = debug_value_.Clone();
  debug_value.Set("document_url", document_url_.spec());
  debug_value.Set("manifest_url", manifest_url_.spec());
  debug_value.Set("expected_id", expected_id_);
  debug_value.Set("manifest_contents", manifest_contents_);
  return base::Value(std::move(debug_value));
}

void InstallPreloadedVerifiedAppCommand::OnShutdown() {
  Abort(CommandResult::kShutdown,
        webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown);
}

void InstallPreloadedVerifiedAppCommand::OnAboutBlankLoaded(
    WebAppUrlLoaderResult result) {
  // The shared web contents must have been reset to about:blank before command
  // execution.
  DCHECK_EQ(web_contents_lock_->shared_web_contents().GetURL(),
            GURL(url::kAboutBlankURL));

  web_contents_lock_->shared_web_contents()
      .GetPrimaryMainFrame()
      ->GetRemoteInterfaces()
      ->GetInterface(manifest_manager_.BindNewPipeAndPassReceiver());
  manifest_manager_.set_disconnect_handler(
      base::BindOnce(&InstallPreloadedVerifiedAppCommand::Abort,
                     weak_ptr_factory_.GetWeakPtr(), CommandResult::kFailure,
                     webapps::InstallResultCode::kWebContentsDestroyed));

  manifest_manager_->ParseManifestFromString(
      document_url_, manifest_url_, manifest_contents_,
      base::BindOnce(&InstallPreloadedVerifiedAppCommand::OnManifestParsed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void InstallPreloadedVerifiedAppCommand::OnManifestParsed(
    blink::mojom::ManifestPtr manifest) {
  // Note that most errors during parsing (e.g. errors to do with parsing a
  // particular field) are silently ignored. As long as the manifest is valid
  // JSON and contains a valid start_url and name, installation will proceed.
  if (blink::IsEmptyManifest(manifest) ||
      !HasRequiredManifestFields(manifest)) {
    Abort(CommandResult::kFailure,
          webapps::InstallResultCode::kNotValidManifestForWebApp);
    return;
  }

  debug_value_.Set("manifest_parsed", true);
  web_app_info_ = std::make_unique<WebAppInstallInfo>(manifest->id);
  web_app_info_->user_display_mode = mojom::UserDisplayMode::kStandalone;

  UpdateWebAppInfoFromManifest(*manifest, manifest_url_, web_app_info_.get());

  base::flat_set<GURL> icon_urls = GetValidIconUrlsToDownload(*web_app_info_);
  base::EraseIf(icon_urls, [](const GURL& url) {
    for (const auto& allowed_host : kHostAllowlist) {
      if (url.DomainIs(allowed_host)) {
        // Found a match, don't erase this url!
        return false;
      }
    }
    // No matches, erase this url!
    return true;
  });

  if (icon_urls.empty()) {
    // Abort if there are no icons to download, so we can distinguish this case
    // from having icons but failing to download them.
    Abort(CommandResult::kFailure,
          webapps::InstallResultCode::kNoValidIconsInManifest);
    return;
  }

  data_retriever_->GetIcons(
      &web_contents_lock_->shared_web_contents(), std::move(icon_urls),
      /*skip_page_favicons=*/true,
      /*fail_all_if_any_fail=*/false,
      base::BindOnce(&InstallPreloadedVerifiedAppCommand::OnIconsRetrieved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void InstallPreloadedVerifiedAppCommand::OnIconsRetrieved(
    IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults icons_http_results) {
  DCHECK(web_app_info_);

  PopulateProductIcons(web_app_info_.get(), &icons_map);
  if (web_app_info_->is_generated_icon) {
    // PopulateProductIcons sets is_generated_icon if it had to generate a
    // product icon due a lack of successfully downloaded product icons. In
    // this case, abort the installation and report the error.
    Abort(CommandResult::kFailure,
          webapps::InstallResultCode::kIconDownloadingFailed);
    return;
  }

  PopulateOtherIcons(web_app_info_.get(), icons_map);

  webapps::AppId app_id =
      GenerateAppIdFromManifestId(web_app_info_->manifest_id);

  if (app_id != expected_id_) {
    Abort(CommandResult::kFailure,
          webapps::InstallResultCode::kExpectedAppIdCheckFailed);
    return;
  }

  app_lock_description_ =
      command_manager()->lock_manager().UpgradeAndAcquireLock(
          std::move(web_contents_lock_), {app_id},
          base::BindOnce(&InstallPreloadedVerifiedAppCommand::OnAppLockAcquired,
                         weak_ptr_factory_.GetWeakPtr()));
}

void InstallPreloadedVerifiedAppCommand::OnAppLockAcquired(
    std::unique_ptr<SharedWebContentsWithAppLock> app_lock) {
  app_lock_ = std::move(app_lock);
  WebAppInstallFinalizer::FinalizeOptions finalize_options(install_source_);
  finalize_options.add_to_quick_launch_bar = false;
  finalize_options.overwrite_existing_manifest_fields = false;
  // TODO(crbug.com/1250011): apply host_allowlist instead of disabling origin
  // association validate for all origins.
  finalize_options.skip_origin_association_validation = true;

  app_lock_->install_finalizer().FinalizeInstall(
      *web_app_info_, finalize_options,
      base::BindOnce(&InstallPreloadedVerifiedAppCommand::OnInstallFinalized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void InstallPreloadedVerifiedAppCommand::OnInstallFinalized(
    const webapps::AppId& app_id,
    webapps::InstallResultCode code,
    OsHooksErrors os_hooks_errors) {
  SignalCompletionAndSelfDestruct(
      webapps::IsSuccess(code) ? CommandResult::kSuccess
                               : CommandResult::kFailure,
      base::BindOnce(std::move(install_callback_), app_id, code));
}

void InstallPreloadedVerifiedAppCommand::Abort(
    CommandResult result,
    webapps::InstallResultCode code) {
  debug_value_.Set("error_code", base::ToString(code));
  SignalCompletionAndSelfDestruct(
      result,
      base::BindOnce(std::move(install_callback_), webapps::AppId(), code));
}

}  // namespace web_app
