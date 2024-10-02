// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_mark.h"

#include "base/json/json_reader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_performance_mark_options.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"

namespace blink {

TEST(PerformanceMarkTest, CreateWithOptions) {
  V8TestingScope scope;

  ExceptionState& exception_state = scope.GetExceptionState();
  ScriptState* script_state = scope.GetScriptState();
  v8::Isolate* isolate = scope.GetIsolate();
  scoped_refptr<SerializedScriptValue> payload_string =
      SerializedScriptValue::Create(String("some-payload"));
  ScriptValue script_value(isolate, payload_string->Deserialize(isolate));

  PerformanceMarkOptions* options = PerformanceMarkOptions::Create();
  options->setDetail(script_value);

  PerformanceMark* pm = PerformanceMark::Create(script_state, "mark-name",
                                                options, exception_state);
  ASSERT_EQ(pm->entryType(), performance_entry_names::kMark);
  ASSERT_EQ(pm->EntryTypeEnum(), PerformanceEntry::EntryType::kMark);
  ASSERT_EQ(payload_string->Deserialize(isolate),
            pm->detail(script_state).V8Value());
}

TEST(PerformanceMarkTest, Construction) {
  V8TestingScope scope;

  ExceptionState& exception_state = scope.GetExceptionState();
  ScriptState* script_state = scope.GetScriptState();
  v8::Isolate* isolate = scope.GetIsolate();

  PerformanceMark* pm = MakeGarbageCollected<PerformanceMark>(
      "mark-name", 0, base::TimeTicks(), SerializedScriptValue::NullValue(),
      exception_state, LocalDOMWindow::From(script_state));
  ASSERT_EQ(pm->entryType(), performance_entry_names::kMark);
  ASSERT_EQ(pm->EntryTypeEnum(), PerformanceEntry::EntryType::kMark);

  ASSERT_EQ(SerializedScriptValue::NullValue()->Deserialize(isolate),
            pm->detail(script_state).V8Value());
  ASSERT_EQ(1u, pm->navigationId());
}

TEST(PerformanceMarkTest, ConstructionWithDetail) {
  V8TestingScope scope;

  ExceptionState& exception_state = scope.GetExceptionState();
  ScriptState* script_state = scope.GetScriptState();
  v8::Isolate* isolate = scope.GetIsolate();
  scoped_refptr<SerializedScriptValue> payload_string =
      SerializedScriptValue::Create(String("some-payload"));

  PerformanceMark* pm = MakeGarbageCollected<PerformanceMark>(
      "mark-name", 0, base::TimeTicks(), payload_string, exception_state,
      LocalDOMWindow::From(script_state));
  ASSERT_EQ(pm->entryType(), performance_entry_names::kMark);
  ASSERT_EQ(pm->EntryTypeEnum(), PerformanceEntry::EntryType::kMark);

  ASSERT_EQ(payload_string->Deserialize(isolate),
            pm->detail(script_state).V8Value());
}

TEST(PerformanceMarkTest, BuildJSONValue) {
  V8TestingScope scope;

  ExceptionState& exception_state = scope.GetExceptionState();
  ScriptState* script_state = scope.GetScriptState();

  const AtomicString expected_name = "mark-name";
  const double expected_start_time = 0;
  const double expected_duration = 0;
  const AtomicString expected_entry_type = "mark";
  PerformanceMark pm(expected_name, expected_start_time, base::TimeTicks(),
                     SerializedScriptValue::NullValue(), exception_state,
                     LocalDOMWindow::From(script_state));

  ScriptValue json_object = pm.toJSONForBinding(script_state);
  EXPECT_TRUE(json_object.IsObject());

  String json_string = ToBlinkString<String>(
      v8::JSON::Stringify(scope.GetContext(),
                          json_object.V8Value().As<v8::Object>())
          .ToLocalChecked(),
      kDoNotExternalize);

  auto parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(json_string.Utf8());
  EXPECT_TRUE(parsed_json->is_dict());

  EXPECT_EQ(expected_name, parsed_json->GetDict().FindString("name")->c_str());
  EXPECT_EQ(expected_entry_type,
            parsed_json->GetDict().FindString("entryType")->c_str());
  EXPECT_EQ(expected_start_time,
            parsed_json->GetDict().FindDouble("startTime").value());
  EXPECT_EQ(expected_duration,
            parsed_json->GetDict().FindDouble("duration").value());

  EXPECT_EQ(5ul, parsed_json->GetDict().size());
}

}  // namespace blink
