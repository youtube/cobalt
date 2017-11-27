// Copyright 2014 Google Inc. All Rights Reserved.
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

#include "base/logging.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/testing/mock_event_listener.h"
#include "cobalt/dom/window.h"
#include "cobalt/script/testing/fake_script_value.h"
#include "cobalt/script/testing/mock_exception_state.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;
using cobalt::dom::EventListener;
using cobalt::dom::testing::MockEventListener;
using cobalt::script::testing::FakeScriptValue;
using cobalt::script::testing::MockExceptionState;

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
    UNREFERENCED_PARAMETER(severity);
    UNREFERENCED_PARAMETER(file);
    UNREFERENCED_PARAMETER(line);
    UNREFERENCED_PARAMETER(message_start);
    *log_interceptor_->output_ += str;
    return true;
  }

 private:
  std::string* output_;
  logging::LogMessageHandlerFunction old_handler_;
  static ScopedLogInterceptor* log_interceptor_;
};

ScopedLogInterceptor* ScopedLogInterceptor::log_interceptor_;

class FakeSettings : public dom::DOMSettings {
 public:
  FakeSettings()
      : dom::DOMSettings(0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                         NULL),
        example_("http://example.com") {}
  const GURL& base_url() const override { return example_; }

 private:
  GURL example_;
};

class MockCspDelegate : public dom::CspDelegateInsecure {
 public:
  MockCspDelegate() {}
  MOCK_CONST_METHOD3(CanLoad,
                     bool(dom::CspDelegate::ResourceType, const GURL&, bool));
};

// Derive from XMLHttpRequest in order to override its csp_delegate.
// Normally this would come from the Document via DOMSettings.
class FakeXmlHttpRequest : public XMLHttpRequest {
 public:
  FakeXmlHttpRequest(script::EnvironmentSettings* settings,
                     dom::CspDelegate* csp_delegate)
      : XMLHttpRequest(settings), csp_delegate_(csp_delegate) {}
  dom::CspDelegate* csp_delegate() const override { return csp_delegate_; }

 private:
  dom::CspDelegate* csp_delegate_;
};

}  // namespace

class XhrTest : public ::testing::Test {
 public:
  dom::DOMSettings* settings() const { return settings_.get(); }

 protected:
  XhrTest();
  ~XhrTest() override;

  scoped_ptr<FakeSettings> settings_;
  scoped_refptr<XMLHttpRequest> xhr_;
  StrictMock<MockExceptionState> exception_state_;
};

XhrTest::XhrTest()
    : settings_(new FakeSettings()), xhr_(new XMLHttpRequest(settings())) {}

XhrTest::~XhrTest() {}

TEST_F(XhrTest, InvalidMethod) {
  scoped_refptr<script::ScriptException> exception;
  EXPECT_CALL(exception_state_, SetException(_))
      .WillOnce(SaveArg<0>(&exception));
  xhr_->Open("INVALID_METHOD", "fake_url", &exception_state_);
  EXPECT_EQ(dom::DOMException::kSyntaxErr,
            dynamic_cast<dom::DOMException*>(exception.get())->code());
}

TEST_F(XhrTest, Open) {
  scoped_ptr<MockEventListener> listener = MockEventListener::Create();
  FakeScriptValue<EventListener> script_object(listener.get());
  xhr_->set_onreadystatechange(script_object);
  EXPECT_CALL(
      *listener,
      HandleEvent(Eq(xhr_), Pointee(Property(&dom::Event::type,
                                             base::Token("readystatechange"))),
                  _))
      .Times(1);
  xhr_->Open("GET", "https://www.google.com", &exception_state_);

  EXPECT_EQ(XMLHttpRequest::kOpened, xhr_->ready_state());
  EXPECT_EQ(GURL("https://www.google.com"), xhr_->request_url());
}

TEST_F(XhrTest, OpenFailConnectSrc) {
  // Verify that opening an XHR to a blocked domain will throw a security
  // exception.
  scoped_refptr<script::ScriptException> exception;
  StrictMock<MockCspDelegate> csp_delegate;
  EXPECT_CALL(exception_state_, SetException(_))
      .WillOnce(SaveArg<0>(&exception));

  xhr_ = new FakeXmlHttpRequest(settings(), &csp_delegate);
  EXPECT_CALL(csp_delegate, CanLoad(_, _, _)).WillOnce(Return(false));
  xhr_->Open("GET", "https://www.google.com", &exception_state_);

  EXPECT_EQ(dom::DOMException::kSecurityErr,
            dynamic_cast<dom::DOMException*>(exception.get())->code());
  EXPECT_EQ(XMLHttpRequest::kUnsent, xhr_->ready_state());
}

TEST_F(XhrTest, OverrideMimeType) {
  EXPECT_EQ("", xhr_->mime_type_override());
  scoped_refptr<script::ScriptException> exception;
  EXPECT_CALL(exception_state_, SetException(_))
      .WillOnce(SaveArg<0>(&exception));

  xhr_->OverrideMimeType("invalidmimetype", &exception_state_);
  EXPECT_EQ("", xhr_->mime_type_override());
  EXPECT_EQ(dom::DOMException::kSyntaxErr,
            dynamic_cast<dom::DOMException*>(exception.get())->code());

  xhr_->OverrideMimeType("text/xml", &exception_state_);
  EXPECT_EQ("text/xml", xhr_->mime_type_override());
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
  EXPECT_EQ(dom::DOMException::kInvalidStateErr,
            dynamic_cast<dom::DOMException*>(exception.get())->code());
}

TEST_F(XhrTest, SetRequestHeader) {
  xhr_->Open("GET", "https://www.google.com", &exception_state_);
  EXPECT_EQ("\r\n", xhr_->request_headers().ToString());

  xhr_->SetRequestHeader("Foo", "bar", &exception_state_);
  EXPECT_EQ("Foo: bar\r\n\r\n", xhr_->request_headers().ToString());
  xhr_->SetRequestHeader("Foo", "baz", &exception_state_);
  EXPECT_EQ("Foo: bar, baz\r\n\r\n", xhr_->request_headers().ToString());
}

TEST_F(XhrTest, GetResponseHeader) {
  xhr_->set_state(XMLHttpRequest::kUnsent);
  EXPECT_EQ(base::nullopt, xhr_->GetResponseHeader("Content-Type"));
  xhr_->set_state(XMLHttpRequest::kOpened);
  EXPECT_EQ(base::nullopt, xhr_->GetResponseHeader("Content-Type"));
  xhr_->set_state(XMLHttpRequest::kHeadersReceived);
  scoped_refptr<net::HttpResponseHeaders> fake_headers(
      new net::HttpResponseHeaders(
          std::string(kFakeHeaders, kFakeHeaders + sizeof(kFakeHeaders))));

  xhr_->set_http_response_headers(fake_headers);
  EXPECT_EQ(base::nullopt, xhr_->GetResponseHeader("Unknown"));
  EXPECT_EQ("text/plain", xhr_->GetResponseHeader("Content-Type").value_or(""));
  EXPECT_EQ("text/plain", xhr_->GetResponseHeader("CONTENT-TYPE").value_or(""));
  EXPECT_EQ("2, 1",
            xhr_->GetResponseHeader("X-CUSTOM-header-COMMA").value_or(""));
}

}  // namespace xhr
}  // namespace cobalt
