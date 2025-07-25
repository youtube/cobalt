// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/v2_authenticator.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "net/base/net_errors.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/authenticator_test_base.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/connection_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

using testing::_;
using testing::DeleteArg;
using testing::SaveArg;

namespace remoting::protocol {

namespace {

const int kMessageSize = 100;
const int kMessages = 1;

const char kTestSharedSecret[] = "1234-1234-5678";
const char kTestSharedSecretBad[] = "0000-0000-0001";

}  // namespace

class V2AuthenticatorTest : public AuthenticatorTestBase {
 public:
  V2AuthenticatorTest() = default;

  V2AuthenticatorTest(const V2AuthenticatorTest&) = delete;
  V2AuthenticatorTest& operator=(const V2AuthenticatorTest&) = delete;

  ~V2AuthenticatorTest() override = default;

 protected:
  void InitAuthenticators(const std::string& client_secret,
                          const std::string& host_secret) {
    host_ = V2Authenticator::CreateForHost(host_cert_, key_pair_, host_secret,
                                           Authenticator::WAITING_MESSAGE);
    client_ = V2Authenticator::CreateForClient(client_secret,
                                               Authenticator::MESSAGE_READY);
  }
};

TEST_F(V2AuthenticatorTest, SuccessfulAuth) {
  ASSERT_NO_FATAL_FAILURE(
      InitAuthenticators(kTestSharedSecret, kTestSharedSecret));
  ASSERT_NO_FATAL_FAILURE(RunAuthExchange());

  ASSERT_EQ(Authenticator::ACCEPTED, host_->state());
  ASSERT_EQ(Authenticator::ACCEPTED, client_->state());

  client_auth_ = client_->CreateChannelAuthenticator();
  host_auth_ = host_->CreateChannelAuthenticator();
  RunChannelAuth(false);

  StreamConnectionTester tester(host_socket_.get(), client_socket_.get(),
                                kMessageSize, kMessages);

  base::RunLoop run_loop;
  tester.Start(run_loop.QuitClosure());
  run_loop.Run();
  tester.CheckResults();
}

// Verify that connection is rejected when secrets don't match.
TEST_F(V2AuthenticatorTest, InvalidSecret) {
  ASSERT_NO_FATAL_FAILURE(
      InitAuthenticators(kTestSharedSecretBad, kTestSharedSecret));
  ASSERT_NO_FATAL_FAILURE(RunAuthExchange());

  ASSERT_EQ(Authenticator::REJECTED, client_->state());

  // Change |client_| so that we can get the last message.
  reinterpret_cast<V2Authenticator*>(client_.get())->state_ =
      Authenticator::MESSAGE_READY;

  std::unique_ptr<jingle_xmpp::XmlElement> message(client_->GetNextMessage());
  ASSERT_TRUE(message.get());

  ASSERT_EQ(Authenticator::WAITING_MESSAGE, client_->state());
  host_->ProcessMessage(message.get(), base::DoNothing());
  // This assumes that V2Authenticator::ProcessMessage runs synchronously.
  ASSERT_EQ(Authenticator::REJECTED, host_->state());
}

}  // namespace remoting::protocol
