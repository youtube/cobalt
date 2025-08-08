// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/transcript_receiver.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/babelorca/proto/tachyon.pb.h"
#include "chromeos/ash/components/boca/babelorca/request_data_wrapper.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_authed_client.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_constants.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_request_data_provider.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_response.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_utils.h"
#include "chromeos/ash/components/boca/babelorca/transcript_builder.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash::babelorca {
namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("ash_babelorca_transcript_receiver",
                                        R"(
        semantics {
          sender: "School Tools"
          description: "Receive teacher speech captions during a School Tool "
                        "session."
          trigger: "User enables receiving speech captions during a School "
                  "Tools session."
          data: "OAuth token for using the instant messaging service."
          user_data {
            type: ACCESS_TOKEN
          }
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "cros-edu-eng@google.com"
            }
          }
          last_reviewed: "2024-10-17"
        }
        policy {
          cookies_allowed: NO
          setting: "This request cannot be stopped in settings, but will not "
                    "be sent if the user does not enable receiving captions in "
                    "School Tools session."
          policy_exception_justification: "Not implemented."
        })");

}  // namespace

TranscriptReceiver::TranscriptReceiver(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    TachyonRequestDataProvider* request_data_provider,
    StreamingClientGetter streaming_client_getter,
    int max_retries)
    : url_loader_factory_(url_loader_factory),
      request_data_provider_(request_data_provider),
      streaming_client_getter_(std::move(streaming_client_getter)),
      max_retries_(max_retries) {}

TranscriptReceiver::~TranscriptReceiver() = default;

void TranscriptReceiver::StartReceiving(OnTranscript on_transcript_cb,
                                        base::OnceClosure on_failure_cb) {
  on_transcript_cb_ = std::move(on_transcript_cb);
  on_failure_cb_ = std::move(on_failure_cb);
  retry_count_ = 0;
  retry_timer_.Stop();
  StartReceivingInternal();
}

void TranscriptReceiver::StartReceivingInternal() {
  if (!request_data_provider_->session_id().has_value() ||
      !request_data_provider_->sender_email().has_value() ||
      !request_data_provider_->tachyon_token().has_value()) {
    LOG(ERROR) << "Session Id is set: "
               << request_data_provider_->session_id().has_value()
               << ", sender email is set: "
               << request_data_provider_->sender_email().has_value()
               << ", tachyon token is set: "
               << request_data_provider_->tachyon_token().has_value();
    std::move(on_failure_cb_).Run();
    return;
  }
  transcript_builder_ = std::make_unique<TranscriptBuilder>(
      request_data_provider_->session_id().value(),
      request_data_provider_->sender_email().value());
  streaming_client_ = streaming_client_getter_.Run(
      url_loader_factory_, base::BindRepeating(&TranscriptReceiver::OnMessage,
                                               base::Unretained(this)));
  ReceiveMessagesRequest request;
  *request.mutable_header() = GetRequestHeaderTemplate();
  request.mutable_header()->set_auth_token_payload(
      request_data_provider_->tachyon_token().value());
  std::unique_ptr<RequestDataWrapper> request_data =
      std::make_unique<RequestDataWrapper>(
          kTrafficAnnotation, kReceiveMessagesUrl, /*max_retries=*/0,
          base::BindOnce(&TranscriptReceiver::OnResponse,
                         base::Unretained(this)));
  streaming_client_->StartAuthedRequestString(std::move(request_data),
                                              request.SerializeAsString());
}

void TranscriptReceiver::OnMessage(mojom::BabelOrcaMessagePtr message) {
  // reset retries.
  retry_count_ = 0;
  std::vector<TranscriptBuilder::Result> results =
      transcript_builder_->GetTranscripts(std::move(message));
  for (auto& result : results) {
    on_transcript_cb_.Run(
        media::SpeechRecognitionResult(std::move(result.text), result.is_final),
        result.language);
  }
}

void TranscriptReceiver::OnResponse(TachyonResponse response) {
  if (response.ok()) {
    VLOG(1) << "Receive stream closed with ok status.";
    retry_count_ = 0;
    StartReceivingInternal();
    return;
  }

  VLOG(1) << "Receive request failed on retry " << retry_count_;
  if (response.status() == TachyonResponse::Status::kAuthError ||
      retry_count_ >= max_retries_) {
    std::move(on_failure_cb_).Run();
    return;
  }
  ++retry_count_;
  constexpr base::TimeDelta kBackoffTime = base::Milliseconds(250);
  retry_timer_.Start(FROM_HERE, kBackoffTime * retry_count_, this,
                     &TranscriptReceiver::StartReceivingInternal);
}

}  // namespace ash::babelorca
