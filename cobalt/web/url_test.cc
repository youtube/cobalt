// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/web/url.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "cobalt/web/testing/gtest_workarounds.h"
#include "cobalt/web/testing/test_with_javascript.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using cobalt::script::testing::MockExceptionState;
using ::testing::_;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace cobalt {
namespace web {
namespace {
class URLTest : public ::testing::Test {
 public:
  URLTest() {}

  StrictMock<MockExceptionState>* exception_state() {
    return &exception_state_;
  }

 private:
  StrictMock<MockExceptionState> exception_state_;
};

class URLTestWithJavaScript : public testing::TestWebWithJavaScript {};
}  // namespace

TEST_F(URLTest, ConstructorWithValidURL) {
  scoped_refptr<script::ScriptException> exception;
  EXPECT_CALL(*exception_state(), SetException(_)).Times(0);
  scoped_refptr<URL> url =
      new URL("https://user:password@www.example.com:1234/foo/bar?baz#qux",
              "about:blank", exception_state());

  EXPECT_EQ("https://user:password@www.example.com:1234/foo/bar?baz#qux",
            url->href());
  EXPECT_EQ("https:", url->protocol());
  EXPECT_EQ("www.example.com:1234", url->host());
  EXPECT_EQ("www.example.com", url->hostname());
  EXPECT_EQ("1234", url->port());
  EXPECT_EQ("/foo/bar", url->pathname());
  EXPECT_EQ("#qux", url->hash());
  EXPECT_EQ("?baz", url->search());
}

TEST_F(URLTest, ConstructorWithInvalidBase) {
  EXPECT_CALL(*exception_state(),
              SetSimpleExceptionVA(script::kTypeError, _, _));
  scoped_refptr<URL> url =
      new URL("https://www.example.com", "", exception_state());

  EXPECT_TRUE(url->href().empty());
}

TEST_F(URLTest, ConstructorWithInvalidURL) {
  EXPECT_CALL(*exception_state(),
              SetSimpleExceptionVA(script::kTypeError, _, _));
  scoped_refptr<URL> url = new URL("...:...", "about:blank", exception_state());

  EXPECT_TRUE(url->href().empty());
}

TEST_F(URLTest, ConstructorWithValidURLAndBase) {
  scoped_refptr<script::ScriptException> exception;
  EXPECT_CALL(*exception_state(), SetException(_)).Times(0);
  scoped_refptr<URL> url = new URL(
      "quux", "https://user:password@www.example.com:1234/foo/bar?baz#qux",
      exception_state());

  EXPECT_EQ("https://user:password@www.example.com:1234/foo/quux", url->href());
  EXPECT_EQ("https:", url->protocol());
  EXPECT_EQ("www.example.com:1234", url->host());
  EXPECT_EQ("www.example.com", url->hostname());
  EXPECT_EQ("1234", url->port());
  EXPECT_EQ("/foo/quux", url->pathname());
  EXPECT_EQ("", url->hash());
  EXPECT_EQ("", url->search());
}

TEST_P(URLTestWithJavaScript, ConstructorWithValidURL) {
  std::string result;
  bool success = EvaluateScript(
      "var url = new "
      "URL('https://user:password@www.example.com:1234/foo/bar?baz#qux');"
      "if (url.href == "
      "'https://user:password@www.example.com:1234/foo/bar?baz#qux' &&"
      "    url.protocol == 'https:' &&"
      "    url.host == 'www.example.com:1234' &&"
      "    url.hostname == 'www.example.com' &&"
      "    url.port == '1234' &&"
      "    url.pathname == '/foo/bar' &&"
      "    url.hash == '#qux' &&"
      "    url.search == '?baz') "
      "    url.href;",
      &result);
  EXPECT_TRUE(success);
  EXPECT_EQ("https://user:password@www.example.com:1234/foo/bar?baz#qux",
            result);

  if (!success) {
    DLOG(ERROR) << "Failed to evaluate test: "
                << "\"" << result << "\"";
  }
}

TEST_P(URLTestWithJavaScript, ConstructorWithInvalidBase) {
  std::string result;
  bool success = EvaluateScript(
      "let result = 'unknown';"
      "try {"
      "  var url = new URL('https://www.example.com', '');"
      "} catch (e) {"
      "  result = e.name;"
      "}"
      "if (!url) result;",
      &result);
  EXPECT_TRUE(success);
  EXPECT_EQ("TypeError", result);

  if (!success) {
    DLOG(ERROR) << "Failed to evaluate test: "
                << "\"" << result << "\"";
  }
}

TEST_P(URLTestWithJavaScript, ConstructorWithInvalidURL) {
  std::string result;
  bool success = EvaluateScript(
      "let result = 'unknown';"
      "try {"
      "  var url = new URL('...:...');"
      "} catch (e) {"
      "  result = e.name;"
      "}"
      "if (!url) result;",
      &result);
  EXPECT_TRUE(success);
  EXPECT_EQ("TypeError", result);

  if (!success) {
    DLOG(ERROR) << "Failed to evaluate test: "
                << "\"" << result << "\"";
  }
}

TEST_P(URLTestWithJavaScript, CreateObjectURL) {
  std::string result;
  bool success = EvaluateScript(
      "let result = 'unknown';"
      "function assert(condition, message) {"
      "  if (!condition) {"
      "    throw new Error(message || \"Assertion failed\");"
      "  }"
      "}"
      "try {"
      "  var blob = new Blob([\" TEST \"]);"
      "  assert(blob.size == 6, \'Blob has wrong size\');"
      "  var url = URL.createObjectURL(blob);"
      "  result = url;"
      "} catch (e) {"
      "  result = e;"
      "}"
      "result;",
      &result);
  EXPECT_TRUE(success);
  EXPECT_GT(result.length(), 5U);
  EXPECT_TRUE(Value(result, ::testing::StartsWith("blob:")));

  if (!success) {
    DLOG(ERROR) << "Failed to evaluate test: "
                << "\"" << result << "\"";
  }
}

INSTANTIATE_TEST_CASE_P(
    URLTestsWithJavaScript, URLTestWithJavaScript,
    ::testing::ValuesIn(testing::TestWebWithJavaScript::GetWebTypes()),
    testing::TestWebWithJavaScript::GetTypeName);


}  // namespace web
}  // namespace cobalt
