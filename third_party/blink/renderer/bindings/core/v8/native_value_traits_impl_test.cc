// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"

#include <utility>

#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_internals.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_sequence_callback.h"
#include "third_party/blink/renderer/core/testing/internals.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

v8::Local<v8::Object> EvaluateScriptForObject(V8TestingScope& scope,
                                              const char* source) {
  v8::Local<v8::Script> script =
      v8::Script::Compile(scope.GetContext(),
                          V8String(scope.GetIsolate(), source))
          .ToLocalChecked();
  return script->Run(scope.GetContext()).ToLocalChecked().As<v8::Object>();
}

v8::Local<v8::Array> EvaluateScriptForArray(V8TestingScope& scope,
                                            const char* source) {
  return EvaluateScriptForObject(scope, source).As<v8::Array>();
}

TEST(NativeValueTraitsImplTest, IDLInterface) {
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;
  Internals* internals = NativeValueTraits<Internals>::NativeValue(
      scope.GetIsolate(), v8::Number::New(scope.GetIsolate(), 42),
      exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ("Failed to convert value to 'Internals'.",
            exception_state.Message());
  EXPECT_EQ(nullptr, internals);
}

TEST(NativeValueTraitsImplTest, IDLRecord) {
  V8TestingScope scope;
  {
    v8::Local<v8::Object> v8_object = v8::Object::New(scope.GetIsolate());
    NonThrowableExceptionState exception_state;
    const auto& record =
        NativeValueTraits<IDLRecord<IDLString, IDLOctet>>::NativeValue(
            scope.GetIsolate(), v8_object, exception_state);
    EXPECT_TRUE(record.empty());
  }
  {
    v8::Local<v8::Object> v8_object =
        EvaluateScriptForObject(scope, "({ foo: 42, bar: -1024 })");

    NonThrowableExceptionState exception_state;
    const auto& record =
        NativeValueTraits<IDLRecord<IDLByteString, IDLLong>>::NativeValue(
            scope.GetIsolate(), v8_object, exception_state);
    EXPECT_EQ(2U, record.size());
    EXPECT_EQ(std::make_pair(String("foo"), int32_t(42)), record[0]);
    EXPECT_EQ(std::make_pair(String("bar"), int32_t(-1024)), record[1]);
  }
  {
    v8::Local<v8::Object> v8_object =
        EvaluateScriptForObject(scope,
                                "Object.defineProperties({}, {"
                                "  foo: {value: 34, enumerable: true},"
                                "  bar: {value: -1024, enumerable: false},"
                                "  baz: {value: 42, enumerable: true},"
                                "})");

    NonThrowableExceptionState exception_state;
    const auto& record =
        NativeValueTraits<IDLRecord<IDLByteString, IDLLong>>::NativeValue(
            scope.GetIsolate(), v8_object, exception_state);
    EXPECT_EQ(2U, record.size());
    EXPECT_EQ(std::make_pair(String("foo"), int32_t(34)), record[0]);
    EXPECT_EQ(std::make_pair(String("baz"), int32_t(42)), record[1]);
  }
  {
    // Exceptions are being thrown in this test, so we need another scope.
    V8TestingScope scope2;
    v8::Local<v8::Object> original_object = EvaluateScriptForObject(
        scope, "(self.originalObject = {foo: 34, bar: 42})");

    NonThrowableExceptionState exception_state;
    const auto& record =
        NativeValueTraits<IDLRecord<IDLString, IDLLong>>::NativeValue(
            scope.GetIsolate(), original_object, exception_state);
    EXPECT_EQ(2U, record.size());

    v8::Local<v8::Proxy> proxy =
        EvaluateScriptForObject(scope,
                                "new Proxy(self.originalObject, {"
                                "  getOwnPropertyDescriptor() {"
                                "    return {"
                                "      configurable: true,"
                                "      get enumerable() { throw 'bogus!'; },"
                                "    };"
                                "  }"
                                "})")
            .As<v8::Proxy>();

    ExceptionState exception_state_from_proxy(
        scope.GetIsolate(), ExceptionState::kExecutionContext,
        "NativeValueTraitsImplTest", "IDLRecordTest");
    const auto& record_from_proxy =
        NativeValueTraits<IDLRecord<IDLString, IDLLong>>::NativeValue(
            scope.GetIsolate(), proxy, exception_state_from_proxy);
    EXPECT_EQ(0U, record_from_proxy.size());
    EXPECT_TRUE(exception_state_from_proxy.HadException());
    EXPECT_TRUE(exception_state_from_proxy.Message().empty());
    v8::Local<v8::Value> v8_exception =
        exception_state_from_proxy.GetException();
    EXPECT_TRUE(v8_exception->IsString());
    EXPECT_TRUE(
        V8String(scope.GetIsolate(), "bogus!")
            ->Equals(
                scope.GetContext(),
                v8_exception->ToString(scope.GetContext()).ToLocalChecked())
            .ToChecked());
  }
  {
    v8::Local<v8::Object> v8_object = EvaluateScriptForObject(
        scope, "({foo: 42, bar: 0, xx: true, abcd: false})");

    NonThrowableExceptionState exception_state;
    const auto& record =
        NativeValueTraits<IDLRecord<IDLByteString, IDLBoolean>>::NativeValue(
            scope.GetIsolate(), v8_object, exception_state);
    EXPECT_EQ(4U, record.size());
    EXPECT_EQ(std::make_pair(String("foo"), true), record[0]);
    EXPECT_EQ(std::make_pair(String("bar"), false), record[1]);
    EXPECT_EQ(std::make_pair(String("xx"), true), record[2]);
    EXPECT_EQ(std::make_pair(String("abcd"), false), record[3]);
  }
  {
    v8::Local<v8::Array> v8_string_array = EvaluateScriptForArray(
        scope, "Object.assign(['Hello, World!', 'Hi, Mom!'], {foo: 'Ohai'})");

    NonThrowableExceptionState exception_state;
    const auto& record =
        NativeValueTraits<IDLRecord<IDLUSVString, IDLString>>::NativeValue(
            scope.GetIsolate(), v8_string_array, exception_state);
    EXPECT_EQ(3U, record.size());
    EXPECT_EQ(std::make_pair(String("0"), String("Hello, World!")), record[0]);
    EXPECT_EQ(std::make_pair(String("1"), String("Hi, Mom!")), record[1]);
    EXPECT_EQ(std::make_pair(String("foo"), String("Ohai")), record[2]);
  }
  {
    v8::Local<v8::Object> v8_object =
        EvaluateScriptForObject(scope, "({[Symbol.toStringTag]: 34, foo: 42})");

    // The presence of symbols should throw a TypeError when the conversion to
    // the record's key type is attempted.
    DummyExceptionStateForTesting exception_state;
    const auto& record =
        NativeValueTraits<IDLRecord<IDLString, IDLShort>>::NativeValue(
            scope.GetIsolate(), v8_object, exception_state);
    EXPECT_TRUE(record.empty());
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_TRUE(exception_state.Message().empty());
  }
  {
    v8::Local<v8::Object> v8_object =
        EvaluateScriptForObject(scope, "Object.create({foo: 34, bar: 512})");

    NonThrowableExceptionState exception_state;
    auto record =
        NativeValueTraits<IDLRecord<IDLString, IDLUnsignedLong>>::NativeValue(
            scope.GetIsolate(), v8_object, exception_state);
    EXPECT_TRUE(record.empty());

    v8_object =
        EvaluateScriptForObject(scope,
                                "Object.assign("
                                "    Object.create({foo: 34, bar: 512}),"
                                "    {quux: 42, foo: 1024})");
    record =
        NativeValueTraits<IDLRecord<IDLString, IDLUnsignedLong>>::NativeValue(
            scope.GetIsolate(), v8_object, exception_state);
    EXPECT_EQ(2U, record.size());
    EXPECT_EQ(std::make_pair(String("quux"), uint32_t(42)), record[0]);
    EXPECT_EQ(std::make_pair(String("foo"), uint32_t(1024)), record[1]);
  }
  {
    v8::Local<v8::Object> v8_object = EvaluateScriptForObject(
        scope, "({foo: ['Hello, World!', 'Hi, Mom!']})");

    NonThrowableExceptionState exception_state;
    const auto& record =
        NativeValueTraits<IDLRecord<IDLString, IDLSequence<IDLString>>>::
            NativeValue(scope.GetIsolate(), v8_object, exception_state);
    EXPECT_EQ(1U, record.size());
    EXPECT_EQ("foo", record[0].first);
    EXPECT_EQ("Hello, World!", record[0].second[0]);
    EXPECT_EQ("Hi, Mom!", record[0].second[1]);
  }
}

TEST(NativeValueTraitsImplTest, IDLSequence) {
  V8TestingScope scope;
  {
    v8::Local<v8::Array> v8_array = v8::Array::New(scope.GetIsolate());
    NonThrowableExceptionState exception_state;
    const auto& sequence =
        NativeValueTraits<IDLSequence<IDLOctet>>::NativeValue(
            scope.GetIsolate(), v8_array, exception_state);
    EXPECT_TRUE(sequence.empty());
  }
  {
    v8::Local<v8::Array> v8_array =
        EvaluateScriptForArray(scope, "[0, 1, 2, 3, 4]");
    NonThrowableExceptionState exception_state;
    const auto& sequence = NativeValueTraits<IDLSequence<IDLLong>>::NativeValue(
        scope.GetIsolate(), v8_array, exception_state);
    EXPECT_EQ(Vector<int32_t>({0, 1, 2, 3, 4}), sequence);
  }
  {
    const double double_pi = 3.141592653589793238;
    const float float_pi = double_pi;
    v8::Local<v8::Array> v8_real_array =
        EvaluateScriptForArray(scope, "[3.141592653589793238]");

    NonThrowableExceptionState exception_state;
    Vector<double> double_vector =
        NativeValueTraits<IDLSequence<IDLDouble>>::NativeValue(
            scope.GetIsolate(), v8_real_array, exception_state);
    EXPECT_EQ(1U, double_vector.size());
    EXPECT_EQ(double_pi, double_vector[0]);

    Vector<float> float_vector =
        NativeValueTraits<IDLSequence<IDLFloat>>::NativeValue(
            scope.GetIsolate(), v8_real_array, exception_state);
    EXPECT_EQ(1U, float_vector.size());
    EXPECT_EQ(float_pi, float_vector[0]);
  }
  {
    v8::Local<v8::Array> v8_array =
        EvaluateScriptForArray(scope, "['Vini, vidi, vici.', 65535, 0.125]");

    NonThrowableExceptionState exception_state;
    HeapVector<ScriptValue> script_value_vector =
        NativeValueTraits<IDLSequence<IDLAny>>::NativeValue(
            scope.GetIsolate(), v8_array, exception_state);
    EXPECT_EQ(3U, script_value_vector.size());
    String report_on_zela;
    EXPECT_TRUE(script_value_vector[0].ToString(report_on_zela));
    EXPECT_EQ("Vini, vidi, vici.", report_on_zela);
    EXPECT_EQ(65535U,
              ToUInt32(scope.GetIsolate(), script_value_vector[1].V8Value(),
                       kNormalConversion, exception_state));
  }
  {
    v8::Local<v8::Array> v8_string_array_array =
        EvaluateScriptForArray(scope, "[['foo', 'bar'], ['x', 'y', 'z']]");

    NonThrowableExceptionState exception_state;
    Vector<Vector<String>> string_vector_vector =
        NativeValueTraits<IDLSequence<IDLSequence<IDLString>>>::NativeValue(
            scope.GetIsolate(), v8_string_array_array, exception_state);
    EXPECT_EQ(2U, string_vector_vector.size());
    EXPECT_EQ(2U, string_vector_vector[0].size());
    EXPECT_EQ("foo", string_vector_vector[0][0]);
    EXPECT_EQ("bar", string_vector_vector[0][1]);
    EXPECT_EQ(3U, string_vector_vector[1].size());
    EXPECT_EQ("x", string_vector_vector[1][0]);
    EXPECT_EQ("y", string_vector_vector[1][1]);
    EXPECT_EQ("z", string_vector_vector[1][2]);
  }
  {
    v8::Local<v8::Array> v8_array =
        EvaluateScriptForArray(scope,
                               "let arr = [1, 2, 3];"
                               "let iterations = ["
                               "  {done: false, value: 8},"
                               "  {done: false, value: 5},"
                               "  {done: true}"
                               "];"
                               "arr[Symbol.iterator] = function() {"
                               "  let i = 0;"
                               "  return {next: () => iterations[i++]};"
                               "}; arr");

    NonThrowableExceptionState exception_state;
    const auto& sequence = NativeValueTraits<IDLSequence<IDLByte>>::NativeValue(
        scope.GetIsolate(), v8_array, exception_state);
    EXPECT_EQ(Vector<int8_t>({1, 2, 3}), sequence);
  }
  {
    v8::Local<v8::Object> v8_object =
        EvaluateScriptForObject(scope,
                                "let obj = {"
                                "  iterations: ["
                                "    {done: false, value: 55},"
                                "    {done: false, value: 0},"
                                "    {done: true, value: 99}"
                                "  ],"
                                "  [Symbol.iterator]() {"
                                "    let i = 0;"
                                "    return {next: () => this.iterations[i++]};"
                                "  }"
                                "}; obj");

    NonThrowableExceptionState exception_state;
    const auto& byte_sequence =
        NativeValueTraits<IDLSequence<IDLByte>>::NativeValue(
            scope.GetIsolate(), v8_object, exception_state);
    EXPECT_EQ(Vector<int8_t>({55, 0}), byte_sequence);
    const auto& boolean_sequence =
        NativeValueTraits<IDLSequence<IDLBoolean>>::NativeValue(
            scope.GetIsolate(), v8_object, exception_state);
    EXPECT_EQ(Vector<bool>({true, false}), boolean_sequence);
  }
}

}  // namespace

}  // namespace blink
