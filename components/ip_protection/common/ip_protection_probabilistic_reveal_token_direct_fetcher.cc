// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_direct_fetcher.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/debug/crash_logging.h"
#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_fetcher.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/net_ipc_param_traits.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "url/gurl.h"
// The ASSIGN_OR_RETURN macro is defined in the both the base::expected code and
// the private-join-and-compute code. We need to undefine the macro here to
// avoid compiler errors.
#undef ASSIGN_OR_RETURN
#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_crypter.h"
#include "components/ip_protection/get_probabilistic_reveal_token.pb.h"
#include "net/base/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"

namespace ip_protection {

namespace {

// TODO(crbug.com/391358219): Add more details.
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation(
        "ip_protection_service_get_probabilistic_reveal_token",
        R"(
    semantics {
      sender: "IP Protection Service Client"
      description:
        "Request to a Google server to obtain probabilistic reveal tokens "
        "for IP Protection proxied origins."
      trigger:
        "On incognito profile startup, and periodically during incognito "
        "session."
      data:
        "None"
      destination: GOOGLE_OWNED_SERVICE
      internal {
        contacts {
          email: "ip-protection-team@google.com"
        }
      }
      user_data {
        type: NONE
      }
      last_reviewed: "2025-01-16"
    }
    policy {
      cookies_allowed: NO
      policy_exception_justification: "Not implemented."
    }
    comments:
      ""
    )");

// The maximum size of a valid serialized GetProbabilisticRevealTokenResponse.
//
// Calculations here are to have a rough estimate and assumes following.
// Calculations are for version 1 (only supported version so far, browser will
// verify this) and curve secp224r1. Exact size depends on how proto messages
// are serialized.
//
// At most 400 tokens are allowed in a single response and browser will verify
// it after deserializing response.
//
// - Assume `bytes` and `repeated` fields use 8 bytes for size.
// - A token takes (4 + 37 + 37) 78 bytes. A response has at most 400 tokens.
//   GetProbabilisticRevealTokenResponse.tokens might take as much as 400*78 + 8
//   (31208) bytes.
// - public_key takes (29 + 8) 37 bytes.
// - expiration_time_seconds take 8 bytes.
// - next_epoch_start_time_seconds take 8 bytes.
// - num_tokens_with_signal takes 4 bytes.
//
// This means response can be as much as (31208 + 37 + 8 + 8 + 4) 31265.
// Limit is set to 32 * 1024 (32768) which gives more than our rough estimate.
//
// Serialized response with 400 tokens size is 26443, obtained by tweaking test
// TryGetProbabilisticRevealTokensLargeResponse.
constexpr size_t kGetProbabilisticRevealTokenResponseMaxBodySize = 32 * 1024;
constexpr char kProtobufContentType[] = "application/x-protobuf";
constexpr int32_t kMinNumberOfTokens = 10;
constexpr int32_t kMaxNumberOfTokens = 400;
constexpr int32_t kTokenVersion = 1;
constexpr int32_t kTokenSize = 29;
constexpr int32_t kMinNumTokensWithSignal = 0;
constexpr base::TimeDelta kMinExpirationTimeDelta = base::Hours(3);
constexpr base::TimeDelta kMaxExpirationTimeDelta = base::Days(3);

network::ResourceRequest CreateFetchRequest() {
  const std::string& get_prt_server_path =
      net::features::kIpPrivacyProbabilisticRevealTokenServerPath.Get();
  GURL::Replacements replacements;
  replacements.SetPathStr(get_prt_server_path);
  network::ResourceRequest resource_request;
  resource_request.url =
      GURL(net::features::kIpPrivacyProbabilisticRevealTokenServer.Get())
          .ReplaceComponents(replacements);
  resource_request.method = net::HttpRequestHeaders::kPostMethod;
  resource_request.credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request.headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                     kProtobufContentType);
  int experiment_arm = net::features::kIpPrivacyDebugExperimentArm.Get();
  if (experiment_arm != 0) {
    resource_request.headers.SetHeader("Ip-Protection-Debug-Experiment-Arm",
                                       base::NumberToString(experiment_arm));
  }
  return resource_request;
}

std::string CreateFetchRequestBody() {
  std::string body;
  GetProbabilisticRevealTokenRequest request;
  request.set_service_type(
      GetProbabilisticRevealTokenRequest_ServiceType_CHROME);
  request.SerializeToString(&body);
  return body;
}

}  // namespace

IpProtectionProbabilisticRevealTokenDirectFetcher::
    IpProtectionProbabilisticRevealTokenDirectFetcher(
        std::unique_ptr<network::PendingSharedURLLoaderFactory>
            pending_url_loader_factory)
    : retriever_(std::move(pending_url_loader_factory)) {}

IpProtectionProbabilisticRevealTokenDirectFetcher::
    ~IpProtectionProbabilisticRevealTokenDirectFetcher() = default;

void IpProtectionProbabilisticRevealTokenDirectFetcher::
    TryGetProbabilisticRevealTokens(
        TryGetProbabilisticRevealTokensCallback callback) {
  // If we are not able to call `RetrieveProbabilisticRevealTokens` yet, return
  // early.
  if (no_get_probabilistic_reveal_tokens_until_ > base::Time::Now()) {
    std::move(callback).Run(
        std::nullopt,
        TryGetProbabilisticRevealTokensResult{
            TryGetProbabilisticRevealTokensStatus::kRequestBackedOff, net::OK,
            no_get_probabilistic_reveal_tokens_until_});
    return;
  }

  retriever_.RetrieveProbabilisticRevealTokens(
      base::BindOnce(&IpProtectionProbabilisticRevealTokenDirectFetcher::
                         OnGetProbabilisticRevealTokensCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

IpProtectionProbabilisticRevealTokenDirectFetcher::Retriever::Retriever(
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory)
    : url_loader_factory_(network::SharedURLLoaderFactory::Create(
          std::move(pending_url_loader_factory))),
      request_(CreateFetchRequest()),
      request_body_(CreateFetchRequestBody()) {
  CHECK(url_loader_factory_);
  CHECK(request_.url.is_valid());
}

IpProtectionProbabilisticRevealTokenDirectFetcher::Retriever::~Retriever() =
    default;

void IpProtectionProbabilisticRevealTokenDirectFetcher::Retriever::
    RetrieveProbabilisticRevealTokens(RetrieveCallback callback) {
  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(
          std::make_unique<network::ResourceRequest>(request_),
          kTrafficAnnotation);

  // Retry on network changes, as sometimes this occurs during browser startup.
  // A network change during DNS resolution results in a DNS error rather than
  // a network change error, so retry in those cases as well.
  url_loader->SetRetryOptions(
      2, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
             network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED);

  url_loader->AttachStringForUpload(request_body_, kProtobufContentType);
  // Get pointer to url_loader before moving it.
  auto* url_loader_ptr = url_loader.get();
  url_loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&Retriever::OnRetrieveProbabilisticRevealTokensCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     // Include the URLLoader in the callback to get error code
                     // and prevent it to go out of scope until the download is
                     // complete.
                     std::move(url_loader), std::move(callback)),
      kGetProbabilisticRevealTokenResponseMaxBodySize);
}

void IpProtectionProbabilisticRevealTokenDirectFetcher::Retriever::
    OnRetrieveProbabilisticRevealTokensCompleted(
        std::unique_ptr<network::SimpleURLLoader> url_loader,
        RetrieveCallback callback,
        std::optional<std::string> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (url_loader->NetError() != net::OK) {
    std::move(callback).Run(base::unexpected(url_loader->NetError()));
    return;
  }
  std::move(callback).Run(std::move(response));
}

// TODO(crbug.com/391358904): add metrics
void IpProtectionProbabilisticRevealTokenDirectFetcher::
    OnGetProbabilisticRevealTokensCompleted(
        TryGetProbabilisticRevealTokensCallback callback,
        base::expected<std::optional<std::string>, int> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!response.has_value() || !response.value().has_value()) {
    // Apply exponential backoff to these sorts of failures.
    no_get_probabilistic_reveal_tokens_until_ =
        base::Time::Now() + next_get_probabilistic_reveal_tokens_backoff_;
    next_get_probabilistic_reveal_tokens_backoff_ *= 2;
  }

  if (!response.has_value()) {
    std::move(callback).Run(
        std::nullopt,
        TryGetProbabilisticRevealTokensResult{
            TryGetProbabilisticRevealTokensStatus::kNetNotOk, response.error(),
            no_get_probabilistic_reveal_tokens_until_});
    // TODO(crbug.com/391358904): add failure metrics using response.error()
    // before returning.
    return;
  } else if (!response.value().has_value()) {
    // url_loader->NetError() is net::OK, however, DownloadToString returned
    // null response.
    std::move(callback).Run(
        std::nullopt,
        TryGetProbabilisticRevealTokensResult{
            TryGetProbabilisticRevealTokensStatus::kNetOkNullResponse, net::OK,
            no_get_probabilistic_reveal_tokens_until_});
    // TODO(crbug.com/391358904): add failure metrics using response.error()
    // before returning.
    return;
  }

  GetProbabilisticRevealTokenResponse response_proto;
  // Parsing is OK since server URL is hard coded in net features.
  if (!response_proto.ParseFromString(response.value().value())) {
    std::move(callback).Run(
        std::nullopt,
        TryGetProbabilisticRevealTokensResult{
            TryGetProbabilisticRevealTokensStatus::kResponseParsingFailed,
            net::OK});
    return;
  }

  if (TryGetProbabilisticRevealTokensStatus status =
          ValidateProbabilisticRevealTokenResponse(response_proto);
      status != TryGetProbabilisticRevealTokensStatus::kSuccess) {
    std::move(callback).Run(
        std::nullopt, TryGetProbabilisticRevealTokensResult{status, net::OK});
    return;
  }

  // Cancel any backoff on success.
  ClearBackoffTimer();

  // TODO(crbug.com/391358904): add success metrics before returning.
  TryGetProbabilisticRevealTokensOutcome outcome;
  for (const auto& t : response_proto.tokens()) {
    outcome.tokens.emplace_back(t.version(), t.u(), t.e());
  }
  outcome.public_key = std::move(response_proto.public_key().y());
  outcome.expiration_time_seconds = response_proto.expiration_time_seconds();
  outcome.next_epoch_start_time_seconds =
      response_proto.next_epoch_start_time_seconds();
  outcome.num_tokens_with_signal = response_proto.num_tokens_with_signal();
  std::move(callback).Run(
      std::optional<TryGetProbabilisticRevealTokensOutcome>{std::move(outcome)},
      TryGetProbabilisticRevealTokensResult{
          TryGetProbabilisticRevealTokensStatus::kSuccess, net::OK});
}

TryGetProbabilisticRevealTokensStatus
IpProtectionProbabilisticRevealTokenDirectFetcher::
    ValidateProbabilisticRevealTokenResponse(
        const GetProbabilisticRevealTokenResponse& response) {
  if (response.tokens_size() < kMinNumberOfTokens) {
    return TryGetProbabilisticRevealTokensStatus::kTooFewTokens;
  }
  if (response.tokens_size() > kMaxNumberOfTokens) {
    return TryGetProbabilisticRevealTokensStatus::kTooManyTokens;
  }
  base::TimeDelta expiration_time_delta =
      base::Time::FromSecondsSinceUnixEpoch(
          response.expiration_time_seconds()) -
      base::Time::Now();
  if (expiration_time_delta < kMinExpirationTimeDelta) {
    return TryGetProbabilisticRevealTokensStatus::kExpirationTooSoon;
  }
  if (expiration_time_delta > kMaxExpirationTimeDelta) {
    return TryGetProbabilisticRevealTokensStatus::kExpirationTooLate;
  }
  if (response.num_tokens_with_signal() < kMinNumTokensWithSignal ||
      response.num_tokens_with_signal() > response.tokens_size()) {
    return TryGetProbabilisticRevealTokensStatus::kInvalidNumTokensWithSignal;
  }
  if (auto crypter = IpProtectionProbabilisticRevealTokenCrypter::Create(
          response.public_key().y(), {});
      !crypter.ok()) {
    return TryGetProbabilisticRevealTokensStatus::kInvalidPublicKey;
  }

  for (const auto& t : response.tokens()) {
    if (t.version() != kTokenVersion) {
      return TryGetProbabilisticRevealTokensStatus::kInvalidTokenVersion;
    }
    if (t.u().size() != kTokenSize || t.e().size() != kTokenSize) {
      return TryGetProbabilisticRevealTokensStatus::kInvalidTokenSize;
    }
  }
  return TryGetProbabilisticRevealTokensStatus::kSuccess;
}

void IpProtectionProbabilisticRevealTokenDirectFetcher::AccountStatusChanged(
    bool account_available) {
  if (account_available) {
    ClearBackoffTimer();
  }
}

void IpProtectionProbabilisticRevealTokenDirectFetcher::ClearBackoffTimer() {
  no_get_probabilistic_reveal_tokens_until_ = base::Time();
  next_get_probabilistic_reveal_tokens_backoff_ =
      kGetProbabilisticRevealTokensFailureTimeout;
}

}  // namespace ip_protection
