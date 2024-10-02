// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_THIRD_PARTY_CLIENT_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_THIRD_PARTY_CLIENT_AUTHENTICATOR_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/client_authentication_config.h"
#include "remoting/protocol/third_party_authenticator_base.h"
#include "remoting/protocol/token_validator.h"

namespace remoting::protocol {

// Implements the client side of the third party authentication mechanism.
// The client authenticator expects a |token_url| and |scope| in the first
// message from the host, then calls the |TokenFetcher| asynchronously to
// request a |token| and |shared_secret| from that url. If the server requires
// interactive authentication, the |TokenFetcher| implementation will show the
// appropriate UI. Once the |TokenFetcher| returns, the client sends the |token|
// to the host, and uses the |shared_secret| to create an underlying
// |V2Authenticator|, which is used to establish the encrypted connection.
class ThirdPartyClientAuthenticator : public ThirdPartyAuthenticatorBase {
 public:
  // Creates a third-party client authenticator.
  // |create_base_authenticator_callback| is used to create the base
  // authenticator. |token_fetcher| is used to get the authentication token.
  ThirdPartyClientAuthenticator(
      const CreateBaseAuthenticatorCallback& create_base_authenticator_callback,
      const FetchThirdPartyTokenCallback& fetch_token_callback);

  ThirdPartyClientAuthenticator(const ThirdPartyClientAuthenticator&) = delete;
  ThirdPartyClientAuthenticator& operator=(
      const ThirdPartyClientAuthenticator&) = delete;

  ~ThirdPartyClientAuthenticator() override;

 protected:
  // ThirdPartyAuthenticator implementation.
  void ProcessTokenMessage(const jingle_xmpp::XmlElement* message,
                           base::OnceClosure resume_callback) override;
  void AddTokenElements(jingle_xmpp::XmlElement* message) override;

 private:
  void OnThirdPartyTokenFetched(
      base::OnceClosure resume_callback,
      const std::string& third_party_token,
      const TokenValidator::ValidationResult& validation_result);

  CreateBaseAuthenticatorCallback create_base_authenticator_callback_;
  FetchThirdPartyTokenCallback fetch_token_callback_;
  std::string token_;

  base::WeakPtrFactory<ThirdPartyClientAuthenticator> weak_factory_{this};
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_THIRD_PARTY_CLIENT_AUTHENTICATOR_H_
