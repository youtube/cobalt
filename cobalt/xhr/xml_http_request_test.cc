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

using cobalt::dom::testing::MockEventListener;
using ::testing::HasSubstr;
using ::testing::Pointee;
using ::testing::Property;

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
  FakeSettings() : dom::DOMSettings(NULL, NULL) {}
  GURL base_url() const OVERRIDE { return GURL("http://example.com"); }
};

}  // namespace

class XhrTest : public ::testing::Test {
 public:
  dom::DOMSettings* settings() const { return settings_.get(); }

 protected:
  XhrTest();
  ~XhrTest() OVERRIDE;

 private:
  scoped_ptr<FakeSettings> settings_;
};

XhrTest::XhrTest() : settings_(new FakeSettings()) {}

XhrTest::~XhrTest() {}

TEST_F(XhrTest, InvalidMethod) {
  scoped_refptr<XMLHttpRequest> xhr = new XMLHttpRequest(settings());
  std::string output;
  {
    ScopedLogInterceptor li(&output);
    xhr->Open("GETT", "fake_url");
  }
  // Note: Once JS exceptions are supported, we will verify that
  // the correct exceptions are thrown. For now we check for log output.
  EXPECT_THAT(output, HasSubstr("TypeError: Invalid method"));
}

TEST_F(XhrTest, Open) {
  scoped_refptr<XMLHttpRequest> xhr = new XMLHttpRequest(settings());
  scoped_refptr<MockEventListener> listener =
      MockEventListener::CreateAsAttribute();
  xhr->set_onreadystatechange(listener);
  EXPECT_CALL(*listener, HandleEvent(Pointee(Property(
                             &dom::Event::type, "readystatechange")))).Times(1);
  xhr->Open("GET", "https://www.google.com");

  EXPECT_EQ(XMLHttpRequest::kOpened, xhr->ready_state());
  EXPECT_EQ(GURL("https://www.google.com"), xhr->request_url());
}

TEST_F(XhrTest, OverrideMimeType) {
  scoped_refptr<XMLHttpRequest> xhr = new XMLHttpRequest(settings());
  EXPECT_EQ("", xhr->mime_type_override());

  std::string output;
  {
    ScopedLogInterceptor li(&output);
    xhr->OverrideMimeType("invalidmimetype");
  }
  EXPECT_THAT(output, HasSubstr("TypeError"));
  EXPECT_EQ("", xhr->mime_type_override());

  xhr->OverrideMimeType("text/xml");
  EXPECT_EQ("text/xml", xhr->mime_type_override());
}

TEST_F(XhrTest, SetResponseType) {
  scoped_refptr<XMLHttpRequest> xhr = new XMLHttpRequest(settings());
  xhr->set_response_type("document");
  EXPECT_EQ("document", xhr->response_type());

  std::string output;
  {
    ScopedLogInterceptor li(&output);
    xhr->set_response_type("something invalid");
  }
  EXPECT_THAT(output, HasSubstr("WARNING"));
}

}  // namespace xhr
}  // namespace cobalt
