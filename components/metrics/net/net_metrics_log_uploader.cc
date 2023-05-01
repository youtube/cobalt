// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/net/net_metrics_log_uploader.h"

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/encrypted_messages/encrypted_message.pb.h"
#include "components/encrypted_messages/message_encrypter.h"
#include "components/metrics/metrics_log_uploader.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/metrics_proto/reporting_info.pb.h"
#include "url/gurl.h"

namespace {

const base::Feature kHttpRetryFeature{"UMAHttpRetry",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// Constants used for encrypting logs that are sent over HTTP. The
// corresponding private key is used by the metrics server to decrypt logs.
const char kEncryptedMessageLabel[] = "metrics log";

const uint8_t kServerPublicKey[] = {
    0x51, 0xcc, 0x52, 0x67, 0x42, 0x47, 0x3b, 0x10, 0xe8, 0x63, 0x18,
    0x3c, 0x61, 0xa7, 0x96, 0x76, 0x86, 0x91, 0x40, 0x71, 0x39, 0x5f,
    0x31, 0x1a, 0x39, 0x5b, 0x76, 0xb1, 0x6b, 0x3d, 0x6a, 0x2b};

const uint32_t kServerPublicKeyVersion = 1;

net::NetworkTrafficAnnotationTag GetNetworkTrafficAnnotation(
    const metrics::MetricsLogUploader::MetricServiceType& service_type) {
  // The code in this function should remain so that we won't need a default
  // case that does not have meaningful annotation.
  if (service_type == metrics::MetricsLogUploader::UMA) {
    return net::DefineNetworkTrafficAnnotation("metrics_report_uma", R"(
        semantics {
          sender: "Metrics UMA Log Uploader"
          description:
            "Report of usage statistics and crash-related data about Chromium. "
            "Usage statistics contain information such as preferences, button "
            "clicks, and memory usage and do not include web page URLs or "
            "personal information. See more at "
            "https://www.google.com/chrome/browser/privacy/ under 'Usage "
            "statistics and crash reports'. Usage statistics are tied to a "
            "pseudonymous machine identifier and not to your email address."
          trigger:
            "Reports are automatically generated on startup and at intervals "
            "while Chromium is running."
          data:
            "A protocol buffer with usage statistics and crash related data."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable or disable this feature by disabling "
            "'Automatically send usage statistics and crash reports to Google' "
            "in Chromium's settings under Advanced Settings, Privacy. The "
            "feature is enabled by default."
          chrome_policy {
            MetricsReportingEnabled {
              policy_options {mode: MANDATORY}
              MetricsReportingEnabled: false
            }
          }
        })");
  }
  DCHECK_EQ(service_type, metrics::MetricsLogUploader::UKM);
  return net::DefineNetworkTrafficAnnotation("metrics_report_ukm", R"(
      semantics {
        sender: "Metrics UKM Log Uploader"
        description:
          "Report of usage statistics that are keyed by URLs to Chromium, "
          "sent only if the profile has History Sync. This includes "
          "information about the web pages you visit and your usage of them, "
          "such as page load speed. This will also include URLs and "
          "statistics related to downloaded files. If Extension Sync is "
          "enabled, these statistics will also include information about "
          "the extensions that have been installed from Chrome Web Store. "
          "Google only stores usage statistics associated with published "
          "extensions, and URLs that are known by Google’s search index. "
          "Usage statistics are tied to a pseudonymous machine identifier "
          "and not to your email address."
        trigger:
          "Reports are automatically generated on startup and at intervals "
          "while Chromium is running with Sync enabled."
        data:
          "A protocol buffer with usage statistics and associated URLs."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting:
          "Users can enable or disable this feature by disabling "
          "'Automatically send usage statistics and crash reports to Google' "
          "in Chromium's settings under Advanced Settings, Privacy. This is "
          "only enabled if all active profiles have History/Extension Sync "
          "enabled without a Sync passphrase."
        chrome_policy {
          MetricsReportingEnabled {
            policy_options {mode: MANDATORY}
            MetricsReportingEnabled: false
          }
        }
      })");
}

std::string SerializeReportingInfo(
    const metrics::ReportingInfo& reporting_info) {
  std::string result;
  std::string bytes;
  bool success = reporting_info.SerializeToString(&bytes);
  DCHECK(success);
  base::Base64Encode(bytes, &result);
  return result;
}

void RecordUploadSizeForServiceTypeHistograms(
    int64_t content_length,
    metrics::MetricsLogUploader::MetricServiceType service_type) {
  switch (service_type) {
    case metrics::MetricsLogUploader::UMA:
      UMA_HISTOGRAM_COUNTS_1M("UMA.LogUploader.UploadSize", content_length);
      break;
    case metrics::MetricsLogUploader::UKM:
      UMA_HISTOGRAM_COUNTS_1M("UKM.LogUploader.UploadSize", content_length);
      break;
  }
}

}  // namespace

namespace metrics {

NetMetricsLogUploader::NetMetricsLogUploader(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::StringPiece server_url,
    base::StringPiece mime_type,
    MetricsLogUploader::MetricServiceType service_type,
    const MetricsLogUploader::UploadCallback& on_upload_complete)
    : url_loader_factory_(std::move(url_loader_factory)),
      server_url_(server_url),
      mime_type_(mime_type.data(), mime_type.size()),
      service_type_(service_type),
      on_upload_complete_(on_upload_complete) {}

NetMetricsLogUploader::NetMetricsLogUploader(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::StringPiece server_url,
    base::StringPiece insecure_server_url,
    base::StringPiece mime_type,
    MetricsLogUploader::MetricServiceType service_type,
    const MetricsLogUploader::UploadCallback& on_upload_complete)
    : url_loader_factory_(std::move(url_loader_factory)),
      server_url_(server_url),
      insecure_server_url_(insecure_server_url),
      mime_type_(mime_type.data(), mime_type.size()),
      service_type_(service_type),
      on_upload_complete_(on_upload_complete) {}

NetMetricsLogUploader::~NetMetricsLogUploader() {
}

void NetMetricsLogUploader::UploadLog(const std::string& compressed_log_data,
                                      const std::string& log_hash,
                                      const ReportingInfo& reporting_info) {
  // If this attempt is a retry, there was a network error, the last attempt was
  // over https, and there is an insecure url set, attempt this upload over
  // HTTP.
  // Currently we only retry over HTTP if the retry-uma-over-http flag is set.
  if (!insecure_server_url_.is_empty() && reporting_info.attempt_count() > 1 &&
      reporting_info.last_error_code() != 0 &&
      reporting_info.last_attempt_was_https() &&
      base::FeatureList::IsEnabled(kHttpRetryFeature)) {
    UploadLogToURL(compressed_log_data, log_hash, reporting_info,
                   insecure_server_url_);
    return;
  }
  UploadLogToURL(compressed_log_data, log_hash, reporting_info, server_url_);
}

void NetMetricsLogUploader::UploadLogToURL(
    const std::string& compressed_log_data,
    const std::string& log_hash,
    const ReportingInfo& reporting_info,
    const GURL& url) {
  DCHECK(!log_hash.empty());

  // TODO(crbug.com/808498): Restore the data use measurement when bug is fixed.

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  // Drop cookies and auth data.
  resource_request->allow_credentials = false;
  resource_request->method = "POST";

  std::string reporting_info_string = SerializeReportingInfo(reporting_info);
  // If we are not using HTTPS for this upload, encrypt it. We do not encrypt
  // requests to localhost to allow testing with a local collector that doesn't
  // have decryption enabled.
  bool should_encrypt =
      !url.SchemeIs(url::kHttpsScheme) && !net::IsLocalhost(url);
  if (should_encrypt) {
    std::string encrypted_hash;
    std::string base64_encoded_hash;
    if (!EncryptString(log_hash, &encrypted_hash)) {
      on_upload_complete_.Run(0, net::ERR_FAILED, false);
      return;
    }
    base::Base64Encode(encrypted_hash, &base64_encoded_hash);
    resource_request->headers.SetHeader("X-Chrome-UMA-Log-SHA1",
                                        base64_encoded_hash);

    std::string encrypted_reporting_info;
    std::string base64_reporting_info;
    if (!EncryptString(reporting_info_string, &encrypted_reporting_info)) {
      on_upload_complete_.Run(0, net::ERR_FAILED, false);
      return;
    }
    base::Base64Encode(encrypted_reporting_info, &base64_reporting_info);
    resource_request->headers.SetHeader("X-Chrome-UMA-ReportingInfo",
                                        base64_reporting_info);
  } else {
    resource_request->headers.SetHeader("X-Chrome-UMA-Log-SHA1", log_hash);
    resource_request->headers.SetHeader("X-Chrome-UMA-ReportingInfo",
                                        reporting_info_string);
    // Tell the server that we're uploading gzipped protobufs only if we are not
    // encrypting, since encrypted messages have to be decrypted server side
    // after decryption, not before.
    resource_request->headers.SetHeader("content-encoding", "gzip");
  }

  url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), GetNetworkTrafficAnnotation(service_type_));

  if (should_encrypt) {
    std::string encrypted_message;
    if (!EncryptString(compressed_log_data, &encrypted_message)) {
      url_loader_.reset();
      on_upload_complete_.Run(0, net::ERR_FAILED, false);
      return;
    }
    url_loader_->AttachStringForUpload(encrypted_message, mime_type_);
    RecordUploadSizeForServiceTypeHistograms(encrypted_message.size(),
                                             service_type_);
  } else {
    url_loader_->AttachStringForUpload(compressed_log_data, mime_type_);
    RecordUploadSizeForServiceTypeHistograms(compressed_log_data.size(),
                                             service_type_);
  }

  // It's safe to use |base::Unretained(this)| here, because |this| owns
  // the |url_loader_|, and the callback will be cancelled if the |url_loader_|
  // is destroyed.
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&NetMetricsLogUploader::OnURLLoadComplete,
                     base::Unretained(this)));
}

// The callback is only invoked if |url_loader_| it was bound against is alive.
void NetMetricsLogUploader::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers)
    response_code = url_loader_->ResponseInfo()->headers->response_code();

  int error_code = url_loader_->NetError();

  bool was_https = url_loader_->GetFinalURL().SchemeIs(url::kHttpsScheme);
  url_loader_.reset();
  on_upload_complete_.Run(response_code, error_code, was_https);
}

bool NetMetricsLogUploader::EncryptString(const std::string& plaintext,
                                          std::string* encrypted) {
  encrypted_messages::EncryptedMessage encrypted_message;
  if (!encrypted_messages::EncryptSerializedMessage(
          kServerPublicKey, kServerPublicKeyVersion, kEncryptedMessageLabel,
          plaintext, &encrypted_message) ||
      !encrypted_message.SerializeToString(encrypted)) {
    return false;
  }
  return true;
}
}  // namespace metrics
