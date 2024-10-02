// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/negotiating_client_authenticator.h"

#include <memory>
#include <sstream>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "remoting/protocol/auth_util.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/pairing_client_authenticator.h"
#include "remoting/protocol/spake2_authenticator.h"
#include "remoting/protocol/v2_authenticator.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting::protocol {

NegotiatingClientAuthenticator::NegotiatingClientAuthenticator(
    const std::string& local_id,
    const std::string& remote_id,
    const ClientAuthenticationConfig& config)
    : NegotiatingAuthenticatorBase(MESSAGE_READY),
      local_id_(local_id),
      remote_id_(remote_id),
      config_(config) {
  if (!config_.fetch_third_party_token_callback.is_null()) {
    AddMethod(Method::THIRD_PARTY_SPAKE2_CURVE25519);
    AddMethod(Method::THIRD_PARTY_SPAKE2_P224);
  }

  AddMethod(Method::PAIRED_SPAKE2_CURVE25519);
  AddMethod(Method::PAIRED_SPAKE2_P224);

  AddMethod(Method::SHARED_SECRET_SPAKE2_CURVE25519);
  AddMethod(Method::SHARED_SECRET_SPAKE2_P224);

  AddMethod(Method::SHARED_SECRET_PLAIN_SPAKE2_P224);
}

NegotiatingClientAuthenticator::~NegotiatingClientAuthenticator() = default;

void NegotiatingClientAuthenticator::ProcessMessage(
    const jingle_xmpp::XmlElement* message,
    base::OnceClosure resume_callback) {
  DCHECK_EQ(state(), WAITING_MESSAGE);
  state_ = PROCESSING_MESSAGE;

  std::string method_attr = message->Attr(kMethodAttributeQName);
  Method method = ParseMethodString(method_attr);

  // The host picked a method different from the one the client had selected.
  if (method != current_method_) {
    // The host must pick a method that is valid and supported by the client,
    // and it must not change methods after it has picked one.
    if (method_set_by_host_ || method == Method::INVALID ||
        !base::Contains(methods_, method)) {
      state_ = REJECTED;
      rejection_reason_ = RejectionReason::PROTOCOL_ERROR;
      std::move(resume_callback).Run();
      return;
    }

    current_method_ = method;
    method_set_by_host_ = true;

    // Copy the message since the authenticator may process it asynchronously.
    CreateAuthenticatorForCurrentMethod(
        WAITING_MESSAGE,
        base::BindOnce(&NegotiatingAuthenticatorBase::ProcessMessageInternal,
                       base::Unretained(this),
                       base::Owned(new jingle_xmpp::XmlElement(*message)),
                       std::move(resume_callback)));
    return;
  }
  ProcessMessageInternal(message, std::move(resume_callback));
}

std::unique_ptr<jingle_xmpp::XmlElement>
NegotiatingClientAuthenticator::GetNextMessage() {
  DCHECK_EQ(state(), MESSAGE_READY);

  // This is the first message to the host, send a list of supported methods.
  if (current_method_ == Method::INVALID) {
    // If no authentication method has been chosen, see if we can optimistically
    // choose one.
    std::unique_ptr<jingle_xmpp::XmlElement> result;
    CreatePreferredAuthenticator();
    if (current_authenticator_) {
      DCHECK(current_authenticator_->state() == MESSAGE_READY);
      result = GetNextMessageInternal();
    } else {
      result = CreateEmptyAuthenticatorMessage();
    }

    if (is_paired()) {
      // If the client is paired with the host then attach pairing client_id to
      // the message.
      jingle_xmpp::XmlElement* pairing_tag =
          new jingle_xmpp::XmlElement(kPairingInfoTag);
      result->AddElement(pairing_tag);
      pairing_tag->AddAttr(kClientIdAttribute, config_.pairing_client_id);
    }

    // Include a list of supported methods.
    std::string supported_methods;
    for (Method method : methods_) {
      if (!supported_methods.empty()) {
        supported_methods += kSupportedMethodsSeparator;
      }
      supported_methods += MethodToString(method);
    }
    result->AddAttr(kSupportedMethodsAttributeQName, supported_methods);
    state_ = WAITING_MESSAGE;
    return result;
  }
  return GetNextMessageInternal();
}

void NegotiatingClientAuthenticator::CreateAuthenticatorForCurrentMethod(
    Authenticator::State preferred_initial_state,
    base::OnceClosure resume_callback) {
  DCHECK_EQ(state(), PROCESSING_MESSAGE);
  DCHECK(current_method_ != Method::INVALID);
  switch (current_method_) {
    case Method::INVALID:
      NOTREACHED();
      break;

    case Method::THIRD_PARTY_SPAKE2_P224:
      current_authenticator_ = std::make_unique<ThirdPartyClientAuthenticator>(
          base::BindRepeating(&V2Authenticator::CreateForClient),
          config_.fetch_third_party_token_callback);
      std::move(resume_callback).Run();
      break;

    case Method::THIRD_PARTY_SPAKE2_CURVE25519:
      current_authenticator_ = std::make_unique<ThirdPartyClientAuthenticator>(
          base::BindRepeating(&Spake2Authenticator::CreateForClient, local_id_,
                              remote_id_),
          config_.fetch_third_party_token_callback);
      std::move(resume_callback).Run();
      break;

    case Method::PAIRED_SPAKE2_P224: {
      PairingClientAuthenticator* pairing_authenticator =
          new PairingClientAuthenticator(
              config_, base::BindRepeating(&V2Authenticator::CreateForClient));
      current_authenticator_ = base::WrapUnique(pairing_authenticator);
      pairing_authenticator->Start(preferred_initial_state,
                                   std::move(resume_callback));
      break;
    }

    case Method::PAIRED_SPAKE2_CURVE25519: {
      PairingClientAuthenticator* pairing_authenticator =
          new PairingClientAuthenticator(
              config_,
              base::BindRepeating(&Spake2Authenticator::CreateForClient,
                                  local_id_, remote_id_));
      current_authenticator_ = base::WrapUnique(pairing_authenticator);
      pairing_authenticator->Start(preferred_initial_state,
                                   std::move(resume_callback));
      break;
    }

    case Method::SHARED_SECRET_PLAIN_SPAKE2_P224:
    case Method::SHARED_SECRET_SPAKE2_P224:
    case Method::SHARED_SECRET_SPAKE2_CURVE25519:
      config_.fetch_secret_callback.Run(
          false,
          base::BindRepeating(
              &NegotiatingClientAuthenticator::CreateSharedSecretAuthenticator,
              weak_factory_.GetWeakPtr(), preferred_initial_state,
              base::Passed(std::move(resume_callback))));
      break;
  }
}

void NegotiatingClientAuthenticator::CreatePreferredAuthenticator() {
  if (is_paired() && base::Contains(methods_, Method::PAIRED_SPAKE2_P224)) {
    PairingClientAuthenticator* pairing_authenticator =
        new PairingClientAuthenticator(
            config_, base::BindRepeating(&V2Authenticator::CreateForClient));
    current_authenticator_ = base::WrapUnique(pairing_authenticator);
    pairing_authenticator->StartPaired(MESSAGE_READY);
    current_method_ = Method::PAIRED_SPAKE2_P224;
  }
}

void NegotiatingClientAuthenticator::CreateSharedSecretAuthenticator(
    Authenticator::State initial_state,
    base::OnceClosure resume_callback,
    const std::string& shared_secret) {
  std::string shared_secret_hash =
      (current_method_ == Method::SHARED_SECRET_PLAIN_SPAKE2_P224)
          ? shared_secret
          : GetSharedSecretHash(config_.host_id, shared_secret);

  if (current_method_ == Method::SHARED_SECRET_SPAKE2_CURVE25519) {
    current_authenticator_ = Spake2Authenticator::CreateForClient(
        local_id_, remote_id_, shared_secret_hash, initial_state);
  } else {
    current_authenticator_ =
        V2Authenticator::CreateForClient(shared_secret_hash, initial_state);
  }
  std::move(resume_callback).Run();
}

bool NegotiatingClientAuthenticator::is_paired() {
  return !config_.pairing_client_id.empty() && !config_.pairing_secret.empty();
}

}  // namespace remoting::protocol
