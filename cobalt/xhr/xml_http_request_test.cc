// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/xhr/xml_http_request.h"

#include <memory>

#include "base/logging.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/testing/stub_environment_settings.h"
#include "cobalt/dom/testing/stub_window.h"
#include "cobalt/dom/window.h"
#include "cobalt/script/testing/fake_script_value.h"
#include "cobalt/script/testing/mock_exception_state.h"
#include "cobalt/web/dom_exception.h"
#include "cobalt/web/event_listener.h"
#include "cobalt/web/testing/mock_event_listener.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using cobalt::script::testing::FakeScriptValue;
using cobalt::script::testing::MockExceptionState;
using cobalt::web::EventListener;
using cobalt::web::testing::MockEventListener;
using ::testing::_;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace cobalt {
namespace xhr {

namespace {

const char kFakeHeaders[] = {
    "HTTP/1.1 200 OK\0"
    "Content-Length:4\0"
    "Content-Type:text/plain\0"
    "Date:Mon, 12 Oct 2015 18:22:16 GMT\0"
    "Server:BaseHTTP/0.3 Python/2.7.6\0"
    "Set-Cookie:test\0"
    "Set-Cookie2:test\0"
    "X-Custom-Header:test\0"
    "X-Custom-Header-Bytes:%E2%80%A6\0"
    "X-Custom-Header-Comma:2\0"
    "X-Custom-Header-Comma:1\0"
    "X-Custom-Header-Empty:\0"};

// Helper class for intercepting DLOG output.
// Not thread safe.
class ScopedLogInterceptor {
 public:
  explicit ScopedLogInterceptor(std::string* output)
      : output_(output), old_handler_(logging::GetLogMessageHandler()) {
    DCHECK(output_);
    DCHECK(!log_interceptor_);
    log_interceptor_ = this;
    logging::SetLogMessageHandler(LogHandler);
  }

  ~ScopedLogInterceptor() {
    logging::SetLogMessageHandler(old_handler_);
    log_interceptor_ = NULL;
  }

  static bool LogHandler(int severity, const char* file, int line,
                         size_t message_start, const std::string& str) {
    *log_interceptor_->output_ += str;
    return true;
  }

 private:
  std::string* output_;
  logging::LogMessageHandlerFunction old_handler_;
  static ScopedLogInterceptor* log_interceptor_;
};

ScopedLogInterceptor* ScopedLogInterceptor::log_interceptor_;

class MockCspDelegate : public web::CspDelegateInsecure {
 public:
  MockCspDelegate() {}
  MOCK_CONST_METHOD3(CanLoad,
                     bool(web::CspDelegate::ResourceType, const GURL&, bool));
};

// Derive from XMLHttpRequestImpl in order to override its csp_delegate.
// Normally this would come from the Document via DOMSettings.
class FakeXmlHttpRequestImpl : public DOMXMLHttpRequestImpl {
 public:
  FakeXmlHttpRequestImpl(xhr::XMLHttpRequest* xhr,
                         web::EnvironmentSettings* settings,
                         web::CspDelegate* csp_delegate)
      : DOMXMLHttpRequestImpl(xhr), csp_delegate_(csp_delegate) {}
  web::CspDelegate* csp_delegate() const override { return csp_delegate_; }

 private:
  web::CspDelegate* csp_delegate_;
};

}  // namespace

class XhrTest : public ::testing::Test {
 public:
  web::EnvironmentSettings* settings() const {
    return stub_window_->environment_settings();
  }

 protected:
  XhrTest();
  ~XhrTest() override;

  std::unique_ptr<dom::testing::StubWindow> stub_window_;
  scoped_refptr<XMLHttpRequest> xhr_;
  StrictMock<MockExceptionState> exception_state_;
};

XhrTest::XhrTest() : stub_window_(new dom::testing::StubWindow()) {
  settings()->set_creation_url(GURL("http://example.com"));
  xhr_ = scoped_refptr<XMLHttpRequest>(new XMLHttpRequest(settings()));
}

XhrTest::~XhrTest() {}

TEST_F(XhrTest, InvalidMethod) {
  scoped_refptr<script::ScriptException> exception;
  EXPECT_CALL(exception_state_, SetException(_))
      .WillOnce(SaveArg<0>(&exception));
  xhr_->Open("INVALID_METHOD", "fake_url", &exception_state_);
  EXPECT_EQ(web::DOMException::kSyntaxErr,
            dynamic_cast<web::DOMException*>(exception.get())->code());
}

TEST_F(XhrTest, Open) {
  std::unique_ptr<MockEventListener> listener = MockEventListener::Create();
  FakeScriptValue<web::EventListener> script_object(listener.get());
  xhr_->xhr_impl_ = std::unique_ptr<FakeXmlHttpRequestImpl>(
      new FakeXmlHttpRequestImpl(xhr_, settings(), NULL));
  xhr_->set_onreadystatechange(script_object);
  EXPECT_CALL(
      *listener,
      HandleEvent(Eq(xhr_),
                  Pointee(Property(&web::Event::type,
                                   base_token::Token("readystatechange"))),
                  _))
      .Times(1);
  xhr_->Open("GET", "https://www.google.com", &exception_state_);

  EXPECT_EQ(XMLHttpRequest::kOpened, xhr_->ready_state());
  EXPECT_EQ(GURL("https://www.google.com"), xhr_->xhr_impl_->request_url());
}

TEST_F(XhrTest, OpenFailConnectSrc) {
  // Verify that opening an XHR to a blocked domain will throw a security
  // exception.
  scoped_refptr<script::ScriptException> exception;
  StrictMock<MockCspDelegate> csp_delegate;
  EXPECT_CALL(exception_state_, SetException(_))
      .WillOnce(SaveArg<0>(&exception));
  xhr_->xhr_impl_ = std::unique_ptr<FakeXmlHttpRequestImpl>(
      new FakeXmlHttpRequestImpl(xhr_, settings(), &csp_delegate));
  EXPECT_CALL(csp_delegate, CanLoad(_, _, _)).WillOnce(Return(false));
  xhr_->Open("GET", "https://www.google.com", &exception_state_);

  EXPECT_EQ(web::DOMException::kSecurityErr,
            dynamic_cast<web::DOMException*>(exception.get())->code());
  EXPECT_EQ(XMLHttpRequest::kUnsent, xhr_->ready_state());
}

TEST_F(XhrTest, OverrideMimeType) {
  EXPECT_EQ("", xhr_->xhr_impl_->mime_type_override());
  scoped_refptr<script::ScriptException> exception;
  EXPECT_CALL(exception_state_, SetException(_))
      .WillOnce(SaveArg<0>(&exception));

  xhr_->OverrideMimeType("invalidmimetype", &exception_state_);
  EXPECT_EQ("", xhr_->xhr_impl_->mime_type_override());
  EXPECT_EQ(web::DOMException::kSyntaxErr,
            dynamic_cast<web::DOMException*>(exception.get())->code());

  xhr_->OverrideMimeType("text/xml", &exception_state_);
  EXPECT_EQ("text/xml", xhr_->xhr_impl_->mime_type_override());
}

TEST_F(XhrTest, SetResponseType) {
  xhr_->set_response_type("document", &exception_state_);
  EXPECT_EQ("document", xhr_->response_type(&exception_state_));

  std::string output;
  {
    ScopedLogInterceptor li(&output);
    xhr_->set_response_type("something invalid", &exception_state_);
  }
  EXPECT_THAT(output, HasSubstr("WARNING"));
}

TEST_F(XhrTest, SetRequestHeaderBeforeOpen) {
  scoped_refptr<script::ScriptException> exception;
  EXPECT_CALL(exception_state_, SetException(_))
      .WillOnce(SaveArg<0>(&exception));
  xhr_->SetRequestHeader("Foo", "bar", &exception_state_);
  EXPECT_EQ(web::DOMException::kInvalidStateErr,
            dynamic_cast<web::DOMException*>(exception.get())->code());
}

TEST_F(XhrTest, SetRequestHeader) {
  xhr_->xhr_impl_ = std::unique_ptr<FakeXmlHttpRequestImpl>(
      new FakeXmlHttpRequestImpl(xhr_, settings(), NULL));
  xhr_->Open("GET", "https://www.google.com", &exception_state_);
  EXPECT_EQ("\r\n", xhr_->xhr_impl_->request_headers().ToString());

  xhr_->SetRequestHeader("Foo", "bar", &exception_state_);
  EXPECT_EQ("Foo: bar\r\n\r\n", xhr_->xhr_impl_->request_headers().ToString());
  xhr_->SetRequestHeader("Foo", "baz", &exception_state_);
  EXPECT_EQ("Foo: bar, baz\r\n\r\n",
            xhr_->xhr_impl_->request_headers().ToString());
}

TEST_F(XhrTest, GetResponseHeader) {
  xhr_->xhr_impl_->set_state(XMLHttpRequest::kUnsent);
  EXPECT_EQ(base::nullopt, xhr_->GetResponseHeader("Content-Type"));
  xhr_->xhr_impl_->set_state(XMLHttpRequest::kOpened);
  EXPECT_EQ(base::nullopt, xhr_->GetResponseHeader("Content-Type"));
  xhr_->xhr_impl_->set_state(XMLHttpRequest::kHeadersReceived);
  scoped_refptr<net::HttpResponseHeaders> fake_headers(
      new net::HttpResponseHeaders(
          std::string(kFakeHeaders, kFakeHeaders + sizeof(kFakeHeaders))));

  xhr_->xhr_impl_->set_http_response_headers(fake_headers);
  EXPECT_EQ(base::nullopt, xhr_->GetResponseHeader("Unknown"));
  EXPECT_EQ("text/plain", xhr_->GetResponseHeader("Content-Type").value_or(""));
  EXPECT_EQ("text/plain", xhr_->GetResponseHeader("CONTENT-TYPE").value_or(""));
  EXPECT_EQ("2, 1",
            xhr_->GetResponseHeader("X-CUSTOM-header-COMMA").value_or(""));
}

}  // namespace xhr
}  // namespace cobalt
