// Copyright 2017 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/websocket/web_socket.h"

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/testing/test_with_javascript.h"
#include "cobalt/dom/window.h"
#include "cobalt/script/script_exception.h"
#include "cobalt/script/testing/mock_exception_state.h"
#include "cobalt/web/context.h"
#include "cobalt/web/dom_exception.h"
#include "testing/gtest/include/gtest/gtest.h"

using cobalt::script::testing::MockExceptionState;
using ::testing::_;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace cobalt {
namespace websocket {

class WebSocketTest : public dom::testing::TestWithJavaScript {
 public:
  web::EnvironmentSettings* settings() {
    return window()->environment_settings();
  }

 protected:
  WebSocketTest() {
    settings()->set_creation_url(GURL("https://example.com"));
    window()->location()->set_url(settings()->creation_url());
  }
};

TEST_F(WebSocketTest, BadOrigin) {
  scoped_refptr<script::ScriptException> exception;

  window()->location()->set_url(GURL());

  EXPECT_CALL(*exception_state(), SetException(_))
      .WillOnce(SaveArg<0>(&exception));

  scoped_refptr<WebSocket> ws(
      new WebSocket(settings(), "ws://example.com", exception_state(), false));

  ASSERT_TRUE(exception.get());
  web::DOMException& dom_exception(
      *base::polymorphic_downcast<web::DOMException*>(exception.get()));
  EXPECT_EQ(web::DOMException::kNone, dom_exception.code());
  EXPECT_EQ(
      dom_exception.message(),
      "Internal error: base_url (the url of the entry script) must be valid.");
}

TEST_F(WebSocketTest, GoodOrigin) {
  struct OriginTestCase {
    const char* const input_base_url;
    const char* const expected_output;
  } origin_test_cases[] = {
      {"https://example.com/", "https://example.com/"},
      {"https://exAMPle.com/", "https://example.com/"},
  };

  EXPECT_CALL(*exception_state(), SetException(_)).Times(0);

  for (std::size_t i(0); i != ARRAYSIZE_UNSAFE(origin_test_cases); ++i) {
    const OriginTestCase& test_case(origin_test_cases[i]);
    std::string new_base = test_case.input_base_url;
    window()->location()->set_url(GURL(new_base));

    scoped_refptr<WebSocket> ws(new WebSocket(settings(), "ws://example.com",
                                              exception_state(), false));

    EXPECT_EQ(ws->entry_script_origin_, test_case.expected_output);
  }
}

TEST_F(WebSocketTest, TestInitialReadyState) {
  scoped_refptr<WebSocket> ws(
      new WebSocket(settings(), "ws://example.com", exception_state(), false));
  EXPECT_EQ(ws->ready_state(), WebSocket::kConnecting);
}

TEST_F(WebSocketTest, SyntaxErrorWhenBadScheme) {
  scoped_refptr<script::ScriptException> exception;

  EXPECT_CALL(*exception_state(), SetException(_))
      .WillOnce(SaveArg<0>(&exception));

  scoped_refptr<WebSocket> ws(new WebSocket(
      settings(), "badscheme://example.com", exception_state(), false));

  web::DOMException& dom_exception(
      *base::polymorphic_downcast<web::DOMException*>(exception.get()));

  ASSERT_TRUE(exception.get());
  EXPECT_EQ(web::DOMException::kSyntaxErr, dom_exception.code());
  EXPECT_EQ(
      dom_exception.message(),
      "Invalid scheme [badscheme].  Only ws, and wss schemes are supported.");
}

TEST_F(WebSocketTest, ParseWsAndWssCorrectly) {
  EXPECT_CALL(*exception_state(), SetException(_)).Times(0);
  scoped_refptr<WebSocket> ws(
      new WebSocket(settings(), "ws://example.com/", exception_state(), false));

  EXPECT_CALL(*exception_state(), SetException(_)).Times(0);
  scoped_refptr<WebSocket> wss(
      new WebSocket(settings(), "wss://example.com", exception_state(), false));
}

TEST_F(WebSocketTest, CheckSecure) {
  EXPECT_CALL(*exception_state(), SetException(_)).Times(0);
  scoped_refptr<WebSocket> ws(
      new WebSocket(settings(), "ws://example.com/", exception_state(), false));
  EXPECT_FALSE(ws->IsSecure());

  EXPECT_CALL(*exception_state(), SetException(_)).Times(0);
  scoped_refptr<WebSocket> wss(
      new WebSocket(settings(), "wss://example.com", exception_state(), false));

  EXPECT_TRUE(wss->IsSecure());
}

// Ithe url string is not an absolute URL, then fail this algorithm.
TEST_F(WebSocketTest, SyntaxErrorWhenRelativeUrl) {
  scoped_refptr<script::ScriptException> exception;

  EXPECT_CALL(*exception_state(), SetException(_))
      .WillOnce(SaveArg<0>(&exception));

  scoped_refptr<WebSocket> ws(
      new WebSocket(settings(), "relative_url", exception_state(), false));

  web::DOMException& dom_exception(
      *base::polymorphic_downcast<web::DOMException*>(exception.get()));
  ASSERT_TRUE(exception.get());
  EXPECT_EQ(web::DOMException::kSyntaxErr, dom_exception.code());
  EXPECT_EQ(dom_exception.message(),
            "Only relative URLs are supported.  [relative_url] is not an "
            "absolute URL.");
}

TEST_F(WebSocketTest, URLHasFragments) {
  scoped_refptr<script::ScriptException> exception;

  EXPECT_CALL(*exception_state(), SetException(_))
      .WillOnce(SaveArg<0>(&exception));

  scoped_refptr<WebSocket> ws(new WebSocket(
      settings(), "wss://example.com/#fragment", exception_state(), false));

  web::DOMException& dom_exception(
      *base::polymorphic_downcast<web::DOMException*>(exception.get()));
  ASSERT_TRUE(exception.get());
  EXPECT_EQ(web::DOMException::kSyntaxErr, dom_exception.code());
  EXPECT_EQ(dom_exception.message(),
            "URL has a fragment 'fragment'.  Fragments are not are supported "
            "in websocket URLs.");
}

TEST_F(WebSocketTest, URLHasPort) {
  scoped_refptr<WebSocket> ws(new WebSocket(settings(), "wss://example.com:789",
                                            exception_state(), false));

  EXPECT_EQ(ws->GetPort(), 789);
  EXPECT_EQ(ws->GetPortAsString(), "789");
}

TEST_F(WebSocketTest, URLHasNoPort) {
  // Per spec:
  // If there is no explicit port, then: if secure is false, let port be 80,
  // otherwise let port be 443.

  scoped_refptr<WebSocket> ws(
      new WebSocket(settings(), "ws://example.com", exception_state(), false));

  EXPECT_EQ(ws->GetPort(), 80);
  EXPECT_EQ(ws->GetPortAsString(), "80");

  scoped_refptr<WebSocket> wss(
      new WebSocket(settings(), "wss://example.com", exception_state(), false));

  EXPECT_EQ(wss->GetPort(), 443);
  EXPECT_EQ(wss->GetPortAsString(), "443");
}

TEST_F(WebSocketTest, ParseHost) {
  // Per spec:
  // Let host be the value of the <host> component of url, converted to ASCII
  // lowercase.
  scoped_refptr<WebSocket> ws(
      new WebSocket(settings(), "wss://eXAmpLe.com", exception_state(), false));

  std::string host(ws->GetHost());
  EXPECT_EQ(host, "example.com");
}

TEST_F(WebSocketTest, ParseResourceName) {
  // Per spec:
  // Let resource name be the value of the <path> component (which might be
  // empty) of
  // url.
  scoped_refptr<WebSocket> ws(new WebSocket(
      settings(), "ws://eXAmpLe.com/resource_name", exception_state(), false));

  std::string resource_name(ws->GetResourceName());
  EXPECT_EQ(resource_name, "/resource_name");
}

TEST_F(WebSocketTest, ParseEmptyResourceName) {
  // Per spec:
  // If resource name is the empty string, set it to a single character U+002F
  // SOLIDUS (/).
  scoped_refptr<WebSocket> ws(
      new WebSocket(settings(), "ws://example.com", exception_state(), false));

  std::string resource_name(ws->GetResourceName());
  EXPECT_EQ(resource_name, "/");
}

TEST_F(WebSocketTest, ParseResourceNameWithQuery) {
  // Per spec:
  // If url has a <query> component, then append a single U+003F QUESTION MARK
  // character (?) to resource name, followed by the value of the <query>
  // component.
  scoped_refptr<WebSocket> ws(
      new WebSocket(settings(), "ws://example.com/resource_name?abc=xyz&j=3",
                    exception_state(), false));

  std::string resource_name(ws->GetResourceName());
  EXPECT_EQ(resource_name, "/resource_name?abc=xyz&j=3");
}

TEST_F(WebSocketTest, FailInsecurePort) {
  scoped_refptr<script::ScriptException> exception;

  EXPECT_CALL(*exception_state(), SetException(_))
      .WillOnce(SaveArg<0>(&exception));

  scoped_refptr<WebSocket> ws(new WebSocket(settings(), "ws://example.com:22",
                                            exception_state(), false));

  web::DOMException& dom_exception(
      *base::polymorphic_downcast<web::DOMException*>(exception.get()));
  ASSERT_TRUE(exception.get());
  EXPECT_EQ(web::DOMException::kSecurityErr, dom_exception.code());
  EXPECT_EQ(dom_exception.message(),
            "Connecting to port 22 using websockets is not allowed.");
}

TEST_F(WebSocketTest, FailInvalidSubProtocols) {
  struct InvalidSubProtocolCase {
    const char* sub_protocol;
    bool exception_thrown;
    web::DOMException::ExceptionCode exception_code;
    const char* error_message;
  } invalid_subprotocol_cases[] = {
      {"a,b", true, web::DOMException::kSyntaxErr,
       "Invalid subprotocol [a,b].  Subprotocols' characters must be in valid "
       "range and not have separating characters.  See RFC 2616 for details."},
      {"a b", true, web::DOMException::kSyntaxErr,
       "Invalid subprotocol [a b].  Subprotocols' characters must be in valid "
       "range and not have separating characters.  See RFC 2616 for details."},
      {"? b", true, web::DOMException::kSyntaxErr,
       "Invalid subprotocol [? b].  Subprotocols' characters must be in valid "
       "range and not have separating characters.  See RFC 2616 for details."},
      {" b", true, web::DOMException::kSyntaxErr,
       "Invalid subprotocol [ b].  Subprotocols' characters must be in valid "
       "range and not have separating characters.  See RFC 2616 for details."},
  };

  for (std::size_t i(0); i != ARRAYSIZE_UNSAFE(invalid_subprotocol_cases);
       ++i) {
    const InvalidSubProtocolCase& test_case(invalid_subprotocol_cases[i]);

    scoped_refptr<script::ScriptException> exception;
    if (test_case.exception_thrown) {
      EXPECT_CALL(*exception_state(), SetException(_))
          .WillOnce(SaveArg<0>(&exception));
    }

    scoped_refptr<WebSocket> ws(new WebSocket(settings(), "ws://example.com",
                                              test_case.sub_protocol,
                                              exception_state(), false));

    if (test_case.exception_thrown) {
      web::DOMException& dom_exception(
          *base::polymorphic_downcast<web::DOMException*>(exception.get()));
      ASSERT_TRUE(exception.get());
      EXPECT_EQ(dom_exception.code(), test_case.exception_code);
      EXPECT_EQ(dom_exception.message(), test_case.error_message);
    }
  }
}

TEST_F(WebSocketTest, SubProtocols) {
  {
    scoped_refptr<script::ScriptException> exception;
    std::vector<std::string> sub_protocols;
    sub_protocols.push_back("chat.example.com");
    sub_protocols.push_back("bicker.example.com");
    scoped_refptr<WebSocket> ws(new WebSocket(settings(), "ws://example.com",
                                              sub_protocols, exception_state(),
                                              false));

    ASSERT_FALSE(exception.get());
  }
  {
    scoped_refptr<script::ScriptException> exception;
    std::string sub_protocol("chat");
    scoped_refptr<WebSocket> ws(new WebSocket(settings(), "ws://example.com",
                                              sub_protocol, exception_state(),
                                              false));

    ASSERT_FALSE(exception.get());
  }
}

TEST_F(WebSocketTest, DuplicatedSubProtocols) {
  scoped_refptr<script::ScriptException> exception;

  EXPECT_CALL(*exception_state(), SetException(_))
      .WillOnce(SaveArg<0>(&exception));
  std::vector<std::string> sub_protocols;
  sub_protocols.push_back("chat");
  sub_protocols.push_back("chat");
  scoped_refptr<WebSocket> ws(new WebSocket(
      settings(), "ws://example.com", sub_protocols, exception_state(), false));

  ASSERT_TRUE(exception.get());

  web::DOMException& dom_exception(
      *base::polymorphic_downcast<web::DOMException*>(exception.get()));
  ASSERT_TRUE(exception.get());
  EXPECT_EQ(dom_exception.code(), web::DOMException::kSyntaxErr);
  EXPECT_EQ(dom_exception.message(), "Subprotocol values must be unique.");
}

}  // namespace websocket
}  // namespace cobalt
