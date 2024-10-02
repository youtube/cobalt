// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_almanac_connector.h"

#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/browser/apps/almanac_api_client/almanac_api_util.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/app_service/package_id.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_wrapper.h"
#include "chrome/browser/apps/app_service/promise_apps/proto/promise_app.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "google_apis/google_api_keys.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

// Endpoint for requesting promise app data on the ChromeOS Almanac API.
constexpr char kPromiseAppAlmanacEndpoint[] = "v1/promise-app/";

// Maximum accepted size of an APS Response. 1MB.
constexpr int kMaxResponseSizeInBytes = 1024 * 1024;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("promise_app_service", R"(
      semantics {
        sender: "Promise App Service"
        description:
          "Sends a request to a Google server to get the name and "
          "icon data of an app being installed on the device."
        trigger:
          "A request can be sent when an app starts installing."
        destination: GOOGLE_OWNED_SERVICE
        internal {
          contacts {
            email: "chromeos-apps-foundation-team@google.com"
          }
        }
        user_data {
          type: PROFILE_DATA
        }
        data: "Name of the app platform and the platform-specific ID of the "
          "app package being installed, e.g. android:com.example.myapp"
        last_reviewed: "2023-04-21"
      }
      policy {
        cookies_allowed: NO
        setting:
          "This request is enabled by app sync without passphrase. You can"
          "disable this request in the 'Sync and Google services' section"
          "in Settings by either: 1. Going into the 'Manage What You Sync'"
          "settings page and turning off Apps sync; OR 2. In the 'Encryption"
          "Options' settings page, select the option to use a sync passphrase."
        policy_exception_justification:
          "This feature is required to deliver core user experiences and "
          "cannot be disabled by policy."
      }
    )");
}  // namespace

namespace apps {

PromiseAppAlmanacConnector::PromiseAppAlmanacConnector(Profile* profile)
    : url_loader_factory_(profile->GetURLLoaderFactory()),
      device_info_manager_(std::make_unique<DeviceInfoManager>(profile)) {}

PromiseAppAlmanacConnector::~PromiseAppAlmanacConnector() = default;

void PromiseAppAlmanacConnector::GetPromiseAppInfo(
    const PackageId& package_id,
    GetPromiseAppCallback callback) {
  if (locale_.empty()) {
    device_info_manager_->GetDeviceInfo(base::BindOnce(
        &PromiseAppAlmanacConnector::SetLocale, weak_ptr_factory_.GetWeakPtr(),
        package_id, std::move(callback)));
  } else {
    GetPromiseAppInfoImpl(package_id, std::move(callback));
  }
}

// static
GURL PromiseAppAlmanacConnector::GetServerUrl() {
  return GURL(base::StrCat({GetAlmanacApiUrl(), kPromiseAppAlmanacEndpoint}));
}

void PromiseAppAlmanacConnector::GetPromiseAppInfoImpl(
    const PackageId& package_id,
    GetPromiseAppCallback callback) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GetServerUrl();
  DCHECK(resource_request->url.is_valid());

  // A POST request is sent with an override to GET due to server requirements.
  resource_request->method = "POST";
  resource_request->headers.SetHeader("X-HTTP-Method-Override", "GET");
  resource_request->headers.SetHeader("X-Goog-Api-Key",
                                      google_apis::GetAPIKey());
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       kTrafficAnnotation);
  auto* loader_ptr = loader.get();
  loader_ptr->AttachStringForUpload(BuildGetPromiseAppRequestBody(package_id),
                                    "application/x-protobuf");

  loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&PromiseAppAlmanacConnector::OnGetPromiseAppResponse,
                     weak_ptr_factory_.GetWeakPtr(), package_id,
                     std::move(loader), std::move(callback)),
      kMaxResponseSizeInBytes);
}

void PromiseAppAlmanacConnector::SetLocale(const PackageId& package_id,
                                           GetPromiseAppCallback callback,
                                           DeviceInfo device_info) {
  locale_ = device_info.locale;
  GetPromiseAppInfoImpl(package_id, std::move(callback));
}

std::string PromiseAppAlmanacConnector::BuildGetPromiseAppRequestBody(
    const apps::PackageId& package_id) {
  apps::proto::PromiseAppRequest request_proto;
  request_proto.set_language(locale_);
  request_proto.set_package_id(package_id.ToString());
  return request_proto.SerializeAsString();
}

void PromiseAppAlmanacConnector::OnGetPromiseAppResponse(
    const PackageId& package_id,
    std::unique_ptr<network::SimpleURLLoader> loader,
    GetPromiseAppCallback callback,
    std::unique_ptr<std::string> response_body) {
  int response_code = 0;
  if (loader->ResponseInfo()) {
    response_code = loader->ResponseInfo()->headers->response_code();
  }
  const int net_error = loader->NetError();

  // HTTP error codes in the 500-599 range represent server errors.
  if (net_error != net::OK || (response_code >= 500 && response_code < 600)) {
    LOG(ERROR) << "Server error. "
               << "Response code: " << response_code
               << ". Net error: " << net::ErrorToString(net_error);
    std::move(callback).Run(absl::nullopt);
    return;
  }

  proto::PromiseAppResponse response;
  if (!response.ParseFromString(*response_body)) {
    LOG(ERROR) << "Parsing failed";
    std::move(callback).Run(absl::nullopt);
    return;
  }
  std::move(callback).Run(PromiseAppWrapper(response));
}

}  // namespace apps
