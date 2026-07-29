// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/cobalt_crash_annotations.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/crashpad/crashpad/client/annotation.h"

namespace cobalt {
namespace browser {

class CobaltCrashAnnotationsTest : public testing::Test {
 protected:
  void SetUp() override { ClearAnnotations(); }

  CobaltCrashAnnotations* annotations() {
    return CobaltCrashAnnotations::GetInstance();
  }

  std::string GetAnnotationValue(const std::string& name) {
    auto it = annotations()->annotations_.find(name);
    if (it == annotations()->annotations_.end()) {
      return "";
    }
    const auto& annotation = *it->second->annotation;
    if (!annotation.is_set()) {
      return "";
    }
    return std::string(annotation.value());
  }

  void ClearAnnotations() { annotations()->ClearAllAnnotations(); }

  bool HasAnnotation(const std::string& name) const {
    return CobaltCrashAnnotations::GetInstance()->annotations_.count(name);
  }
};

TEST_F(CobaltCrashAnnotationsTest, SetAnnotations) {
  std::string client_context = R"({"test": "data"})";
  annotations()->SetAnnotation("clientContext", client_context);
  EXPECT_TRUE(HasAnnotation("clientContext"));
  EXPECT_EQ(GetAnnotationValue("clientContext"), client_context);

  std::string session_id = "test-session-id";
  annotations()->SetAnnotation("customSessionId", session_id);
  EXPECT_TRUE(HasAnnotation("customSessionId"));
  EXPECT_EQ(GetAnnotationValue("customSessionId"), session_id);

  std::string is_backgrounded = "1";
  annotations()->SetAnnotation("isBackgrounded", is_backgrounded);
  EXPECT_TRUE(HasAnnotation("isBackgrounded"));
  EXPECT_EQ(GetAnnotationValue("isBackgrounded"), is_backgrounded);
}

TEST_F(CobaltCrashAnnotationsTest, ClearAnnotations) {
  annotations()->SetAnnotation("key1", "value1");
  annotations()->SetAnnotation("key2", "value2");
  EXPECT_TRUE(HasAnnotation("key1"));
  EXPECT_TRUE(HasAnnotation("key2"));

  // Explicitly clear key1.
  annotations()->ClearAnnotation("key1");
  // The entry remains in the map but the Crashpad value is empty.
  EXPECT_TRUE(HasAnnotation("key1"));
  EXPECT_EQ(GetAnnotationValue("key1"), "");

  // Clearing non-existent key should not crash.
  annotations()->ClearAnnotation("key3");

  // Clear all annotations.
  annotations()->ClearAllAnnotations();
  EXPECT_FALSE(HasAnnotation("key1"));
  EXPECT_FALSE(HasAnnotation("key2"));
}

TEST_F(CobaltCrashAnnotationsTest, SetEmptyClears) {
  annotations()->SetAnnotation("key", "value");
  EXPECT_TRUE(HasAnnotation("key"));

  annotations()->SetAnnotation("key", "");
  EXPECT_TRUE(HasAnnotation("key"));
  EXPECT_EQ(GetAnnotationValue("key"), "");
}

TEST_F(CobaltCrashAnnotationsTest, GenericNames) {
  std::string my_value = "my-custom-value";
  annotations()->SetAnnotation("myCustomName", my_value);
  EXPECT_TRUE(HasAnnotation("myCustomName"));
  EXPECT_EQ(GetAnnotationValue("myCustomName"), my_value);

  std::string updated_value = "updated-value";
  annotations()->SetAnnotation("myCustomName", updated_value);
  EXPECT_EQ(GetAnnotationValue("myCustomName"), updated_value);
}

TEST_F(CobaltCrashAnnotationsTest, MaxAnnotations) {
  for (size_t i = 0; i < CobaltCrashAnnotations::kMaxAnnotations; ++i) {
    std::string name = "name" + std::to_string(i);
    annotations()->SetAnnotation(name, "value");
    EXPECT_TRUE(HasAnnotation(name));
  }

  // The next one should fail.
  annotations()->SetAnnotation("extraName", "extraValue");
  EXPECT_FALSE(HasAnnotation("extraName"));
}

}  // namespace browser
}  // namespace cobalt
