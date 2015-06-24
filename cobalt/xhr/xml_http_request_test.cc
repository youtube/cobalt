/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/xhr/xml_http_request.h"

#include "base/logging.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/testing/mock_event_listener.h"
#include "cobalt/dom/window.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::HasSubstr;
using ::testing::Pointee;
using ::testing::Property;
using cobalt::dom::testing::MockEventListener;

namespace cobalt {
namespace xhr {

namespace {

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
  logging::LogMessageHandlerFunction old_handler_;
  std::string* output_;
  static ScopedLogInterceptor* log_interceptor_;
};

ScopedLogInterceptor* ScopedLogInterceptor::log_interceptor_;

class FakeSettings : public dom::DOMSettings {
 public:
  FakeSettings() : dom::DOMSettings(NULL, NULL, NULL) {}
  GURL base_url() const OVERRIDE { return GURL("http://example.com"); }
};

}  // namespace

class XhrTest : public ::testing::Test {
 public:
  dom::DOMSettings* settings() const { return settings_.get(); }

 protected:
  XhrTest();
  ~XhrTest() OVERRIDE;

  scoped_ptr<FakeSettings> settings_;
  scoped_refptr<XMLHttpRequest> xhr_;
};

XhrTest::XhrTest()
    : settings_(new FakeSettings()), xhr_(new XMLHttpRequest(settings())) {}

XhrTest::~XhrTest() {}

TEST_F(XhrTest, InvalidMethod) {
  std::string output;
  {
    ScopedLogInterceptor li(&output);
    xhr_->Open("INVALID_METHOD", "fake_url");
  }
  // Note: Once JS exceptions are supported, we will verify that
  // the correct exceptions are thrown. For now we check for log output.
  EXPECT_THAT(output, HasSubstr("TypeError: Invalid method"));
}

TEST_F(XhrTest, Open) {
  scoped_refptr<MockEventListener> listener =
      MockEventListener::CreateAsAttribute();
  xhr_->set_onreadystatechange(listener);
  EXPECT_CALL(*listener, HandleEvent(Pointee(Property(
                             &dom::Event::type, "readystatechange")))).Times(1);
  xhr_->Open("GET", "https://www.google.com");

  EXPECT_EQ(XMLHttpRequest::kOpened, xhr_->ready_state());
  EXPECT_EQ(GURL("https://www.google.com"), xhr_->request_url());
}

TEST_F(XhrTest, OverrideMimeType) {
  EXPECT_EQ("", xhr_->mime_type_override());

  std::string output;
  {
    ScopedLogInterceptor li(&output);
    xhr_->OverrideMimeType("invalidmimetype");
  }
  EXPECT_THAT(output, HasSubstr("TypeError"));
  EXPECT_EQ("", xhr_->mime_type_override());

  xhr_->OverrideMimeType("text/xml");
  EXPECT_EQ("text/xml", xhr_->mime_type_override());
}

TEST_F(XhrTest, SetResponseType) {
  xhr_->set_response_type("document");
  EXPECT_EQ("document", xhr_->response_type());

  std::string output;
  {
    ScopedLogInterceptor li(&output);
    xhr_->set_response_type("something invalid");
  }
  EXPECT_THAT(output, HasSubstr("WARNING"));
}

TEST_F(XhrTest, SetRequestHeaderBeforeOpen) {
  std::string output;
  {
    ScopedLogInterceptor li(&output);
    xhr_->SetRequestHeader("Foo", "bar");
  }
  EXPECT_THAT(output, HasSubstr("InvalidStateError"));
}

TEST_F(XhrTest, SetRequestHeader) {
  xhr_->Open("GET", "https://www.google.com");
  EXPECT_EQ("\r\n", xhr_->request_headers().ToString());

  xhr_->SetRequestHeader("Foo", "bar");
  EXPECT_EQ("Foo: bar\r\n\r\n", xhr_->request_headers().ToString());
  xhr_->SetRequestHeader("Foo", "baz");
  EXPECT_EQ("Foo: bar, baz\r\n\r\n", xhr_->request_headers().ToString());
}

}  // namespace xhr
}  // namespace cobalt
