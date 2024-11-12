// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/tachyon_registrar.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/boca/babelorca/proto/tachyon.pb.h"
#include "chromeos/ash/components/boca/babelorca/request_data_wrapper.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_authed_client.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_constants.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_response.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_utils.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace ash::babelorca {
namespace {

constexpr int kMaxRetries = 3;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("ash_babelorca_tachyon_registrar", R"(
        semantics {
          sender: "School Tools"
          description: "Sign into Google instant messaging service so that "
                        "captions be sent and received during a School Tools "
                        "session."
          trigger: "User enables sending or recieivng captions during a School "
                    "Tools session"
          data: "User oauth token for using the instant messaging service."
          user_data {
            type: ACCESS_TOKEN
          }
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "cros-edu-eng@google.com"
            }
          }
          last_reviewed: "2024-10-15"
        }
        policy {
          cookies_allowed: NO
          setting: "This request cannot be stopped in settings, but will not "
                    "be sent if the user does not enable session captions in "
                    "School Tools."
          policy_exception_justification: "Not implemented."
        })");

}  // namespace

TachyonRegistrar::TachyonRegistrar(TachyonAuthedClient* authed_client)
    : authed_client_(authed_client) {}

TachyonRegistrar::~TachyonRegistrar() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TachyonRegistrar::Register(const std::string& client_uuid,
                                base::OnceCallback<void(bool)> success_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto signin_request = std::make_unique<SignInGaiaRequest>();
  signin_request->set_app(kTachyonAppName);
  // Request header
  RequestHeader request_header = GetRequestHeaderTemplate();
  *signin_request->mutable_header() = std::move(request_header);
  // Register data
  RegisterData* register_data = signin_request->mutable_register_data();
  register_data->mutable_device_id()->set_id(client_uuid);
  register_data->mutable_device_id()->set_type(DeviceIdType::CLIENT_UUID);

  auto response_callback =
      base::BindOnce(&TachyonRegistrar::OnResponse,
                     weak_ptr_factory.GetWeakPtr(), std::move(success_cb));

  authed_client_->StartAuthedRequest(
      std::make_unique<RequestDataWrapper>(kTrafficAnnotation, kSigninGaiaUrl,
                                           kMaxRetries,
                                           std::move(response_callback)),
      std::move(signin_request));
}

std::optional<std::string> TachyonRegistrar::GetTachyonToken() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tachyon_token_;
}

void TachyonRegistrar::ResetToken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tachyon_token_.reset();
}

void TachyonRegistrar::OnResponse(base::OnceCallback<void(bool)> success_cb,
                                  TachyonResponse response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!response.ok()) {
    std::move(success_cb).Run(false);
    return;
  }
  SignInGaiaResponse signin_response;
  if (!signin_response.ParseFromString(response.response_body())) {
    LOG(ERROR) << "Unable to parse Tachyon response";
    std::move(success_cb).Run(false);
    return;
  }
  tachyon_token_ = std::move(signin_response.auth_token().payload());
  std::move(success_cb).Run(true);
}

}  // namespace ash::babelorca
