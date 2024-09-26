/*
 * Copyright (C) 2017 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"

#include "base/debug/dump_without_crashing.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event_target.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_link_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_window.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/shadow_realm/shadow_realm_global_scope.h"
#include "third_party/blink/renderer/core/typed_arrays/flexible_array_buffer_view.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

void V8SetReturnValue(const v8::PropertyCallbackInfo<v8::Value>& info,
                      const v8::PropertyDescriptor& descriptor) {
  DCHECK(descriptor.has_configurable());
  DCHECK(descriptor.has_enumerable());
  if (descriptor.has_value()) {
    // Data property
    DCHECK(descriptor.has_writable());
    info.GetReturnValue().Set(
        V8ObjectBuilder(ScriptState::ForCurrentRealm(info))
            .Add("configurable", descriptor.configurable())
            .Add("enumerable", descriptor.enumerable())
            .Add("value", descriptor.value())
            .Add("writable", descriptor.writable())
            .V8Value());
    return;
  }
  // Accessor property
  DCHECK(descriptor.has_get() || descriptor.has_set());
  info.GetReturnValue().Set(V8ObjectBuilder(ScriptState::ForCurrentRealm(info))
                                .Add("configurable", descriptor.configurable())
                                .Add("enumerable", descriptor.enumerable())
                                .Add("get", descriptor.get())
                                .Add("set", descriptor.set())
                                .V8Value());
}

const int32_t kMaxInt32 = 0x7fffffff;
const int32_t kMinInt32 = -kMaxInt32 - 1;
const uint32_t kMaxUInt32 = 0xffffffff;
const int64_t kJSMaxInteger =
    0x20000000000000LL -
    1;  // 2^53 - 1, maximum uniquely representable integer in ECMAScript.

static double EnforceRange(double x,
                           double minimum,
                           double maximum,
                           const char* type_name,
                           ExceptionState& exception_state) {
  if (!std::isfinite(x)) {
    exception_state.ThrowTypeError(
        "Value is" + String(std::isinf(x) ? " infinite and" : "") +
        " not of type '" + String(type_name) + "'.");
    return 0;
  }
  x = trunc(x);
  if (x < minimum || x > maximum) {
    exception_state.ThrowTypeError("Value is outside the '" +
                                   String(type_name) + "' value range.");
    return 0;
  }
  return x;
}

template <typename T>
struct IntTypeNumberOfValues {
  static constexpr unsigned value =
      1 << (std::numeric_limits<T>::digits + std::is_signed<T>::value);
};

template <typename T>
struct IntTypeLimits {};

template <>
struct IntTypeLimits<int8_t> {
  static constexpr int8_t kMinValue = std::numeric_limits<int8_t>::min();
  static constexpr int8_t kMaxValue = std::numeric_limits<int8_t>::max();
  static constexpr unsigned kNumberOfValues =
      IntTypeNumberOfValues<int8_t>::value;  // 2^8
};

template <>
struct IntTypeLimits<uint8_t> {
  static constexpr uint8_t kMaxValue = std::numeric_limits<uint8_t>::max();
  static constexpr unsigned kNumberOfValues =
      IntTypeNumberOfValues<uint8_t>::value;  // 2^8
};

template <>
struct IntTypeLimits<int16_t> {
  static constexpr int16_t kMinValue = std::numeric_limits<int16_t>::min();
  static constexpr int16_t kMaxValue = std::numeric_limits<int16_t>::max();
  static constexpr unsigned kNumberOfValues =
      IntTypeNumberOfValues<int16_t>::value;  // 2^16
};

template <>
struct IntTypeLimits<uint16_t> {
  static constexpr uint16_t kMaxValue = std::numeric_limits<uint16_t>::max();
  static constexpr unsigned kNumberOfValues =
      IntTypeNumberOfValues<uint16_t>::value;  // 2^16
};

template <typename T>
static inline T ToSmallerInt(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             IntegerConversionConfiguration configuration,
                             const char* type_name,
                             ExceptionState& exception_state) {
  typedef IntTypeLimits<T> LimitsTrait;

  // Fast case. The value is already a 32-bit integer in the right range.
  if (value->IsInt32()) {
    int32_t result = value.As<v8::Int32>()->Value();
    if (result >= LimitsTrait::kMinValue && result <= LimitsTrait::kMaxValue)
      return static_cast<T>(result);
    if (configuration == kEnforceRange) {
      exception_state.ThrowTypeError("Value is outside the '" +
                                     String(type_name) + "' value range.");
      return 0;
    }
    if (configuration == kClamp)
      return ClampTo<T>(result);
    result %= LimitsTrait::kNumberOfValues;
    return static_cast<T>(result > LimitsTrait::kMaxValue
                              ? result - LimitsTrait::kNumberOfValues
                              : result);
  }

  v8::Local<v8::Number> number_object;
  if (value->IsNumber()) {
    number_object = value.As<v8::Number>();
  } else {
    // Can the value be converted to a number?
    v8::TryCatch block(isolate);
    if (!value->ToNumber(isolate->GetCurrentContext())
             .ToLocal(&number_object)) {
      exception_state.RethrowV8Exception(block.Exception());
      return 0;
    }
  }
  DCHECK(!number_object.IsEmpty());

  if (configuration == kEnforceRange) {
    return EnforceRange(number_object->Value(), LimitsTrait::kMinValue,
                        LimitsTrait::kMaxValue, type_name, exception_state);
  }

  double number_value = number_object->Value();
  if (std::isnan(number_value) || !number_value)
    return 0;

  if (configuration == kClamp)
    return ClampTo<T>(number_value);

  if (std::isinf(number_value))
    return 0;

  // Confine number to (-kNumberOfValues, kNumberOfValues).
  number_value =
      number_value < 0 ? -floor(fabs(number_value)) : floor(fabs(number_value));
  number_value = fmod(number_value, LimitsTrait::kNumberOfValues);

  // Adjust range to [-kMinValue, kMaxValue].
  if (number_value < LimitsTrait::kMinValue)
    number_value += LimitsTrait::kNumberOfValues;
  else if (LimitsTrait::kMaxValue < number_value)
    number_value -= LimitsTrait::kNumberOfValues;

  return static_cast<T>(number_value);
}

template <typename T>
static inline T ToSmallerUInt(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              IntegerConversionConfiguration configuration,
                              const char* type_name,
                              ExceptionState& exception_state) {
  typedef IntTypeLimits<T> LimitsTrait;

  // Fast case. The value is a 32-bit signed integer - possibly positive?
  if (value->IsInt32()) {
    int32_t result = value.As<v8::Int32>()->Value();
    if (result >= 0 && result <= LimitsTrait::kMaxValue)
      return static_cast<T>(result);
    if (configuration == kEnforceRange) {
      exception_state.ThrowTypeError("Value is outside the '" +
                                     String(type_name) + "' value range.");
      return 0;
    }
    if (configuration == kClamp)
      return ClampTo<T>(result);
    return static_cast<T>(result);
  }

  v8::Local<v8::Number> number_object;
  if (value->IsNumber()) {
    number_object = value.As<v8::Number>();
  } else {
    // Can the value be converted to a number?
    v8::TryCatch block(isolate);
    if (!value->ToNumber(isolate->GetCurrentContext())
             .ToLocal(&number_object)) {
      exception_state.RethrowV8Exception(block.Exception());
      return 0;
    }
  }
  DCHECK(!number_object.IsEmpty());

  if (configuration == kEnforceRange) {
    return EnforceRange(number_object->Value(), 0, LimitsTrait::kMaxValue,
                        type_name, exception_state);
  }

  double number_value = number_object->Value();

  if (std::isnan(number_value) || !number_value)
    return 0;

  if (configuration == kClamp)
    return ClampTo<T>(number_value);

  if (std::isinf(number_value))
    return 0;

  // Confine number to (-kNumberOfValues, kNumberOfValues).
  double number = fmod(trunc(number_value), LimitsTrait::kNumberOfValues);

  // Adjust range to [0, kNumberOfValues).
  if (number < 0)
    number += LimitsTrait::kNumberOfValues;

  return static_cast<T>(number);
}

int8_t ToInt8(v8::Isolate* isolate,
              v8::Local<v8::Value> value,
              IntegerConversionConfiguration configuration,
              ExceptionState& exception_state) {
  return ToSmallerInt<int8_t>(isolate, value, configuration, "byte",
                              exception_state);
}

uint8_t ToUInt8(v8::Isolate* isolate,
                v8::Local<v8::Value> value,
                IntegerConversionConfiguration configuration,
                ExceptionState& exception_state) {
  return ToSmallerUInt<uint8_t>(isolate, value, configuration, "octet",
                                exception_state);
}

int16_t ToInt16(v8::Isolate* isolate,
                v8::Local<v8::Value> value,
                IntegerConversionConfiguration configuration,
                ExceptionState& exception_state) {
  return ToSmallerInt<int16_t>(isolate, value, configuration, "short",
                               exception_state);
}

uint16_t ToUInt16(v8::Isolate* isolate,
                  v8::Local<v8::Value> value,
                  IntegerConversionConfiguration configuration,
                  ExceptionState& exception_state) {
  return ToSmallerUInt<uint16_t>(isolate, value, configuration,
                                 "unsigned short", exception_state);
}

int32_t ToInt32Slow(v8::Isolate* isolate,
                    v8::Local<v8::Value> value,
                    IntegerConversionConfiguration configuration,
                    ExceptionState& exception_state) {
  DCHECK(!value->IsInt32());
  // Can the value be converted to a number?
  v8::TryCatch block(isolate);
  v8::Local<v8::Number> number_object;
  if (!value->ToNumber(isolate->GetCurrentContext()).ToLocal(&number_object)) {
    exception_state.RethrowV8Exception(block.Exception());
    return 0;
  }

  DCHECK(!number_object.IsEmpty());

  double number_value = number_object->Value();
  if (configuration == kEnforceRange) {
    return EnforceRange(number_value, kMinInt32, kMaxInt32, "long",
                        exception_state);
  }

  if (std::isnan(number_value))
    return 0;

  if (configuration == kClamp)
    return ClampTo<int32_t>(number_value);

  if (std::isinf(number_value))
    return 0;

  int32_t result;
  if (!number_object->Int32Value(isolate->GetCurrentContext()).To(&result)) {
    exception_state.RethrowV8Exception(block.Exception());
    return 0;
  }
  return result;
}

uint32_t ToUInt32Slow(v8::Isolate* isolate,
                      v8::Local<v8::Value> value,
                      IntegerConversionConfiguration configuration,
                      ExceptionState& exception_state) {
  DCHECK(!value->IsUint32());
  if (value->IsInt32()) {
    DCHECK_NE(configuration, kNormalConversion);
    int32_t result = value.As<v8::Int32>()->Value();
    if (result >= 0)
      return result;
    if (configuration == kEnforceRange) {
      exception_state.ThrowTypeError(
          "Value is outside the 'unsigned long' value range.");
      return 0;
    }
    DCHECK_EQ(configuration, kClamp);
    return ClampTo<uint32_t>(result);
  }

  // Can the value be converted to a number?
  v8::TryCatch block(isolate);
  v8::Local<v8::Number> number_object;
  if (!value->ToNumber(isolate->GetCurrentContext()).ToLocal(&number_object)) {
    exception_state.RethrowV8Exception(block.Exception());
    return 0;
  }
  DCHECK(!number_object.IsEmpty());

  if (configuration == kEnforceRange) {
    return EnforceRange(number_object->Value(), 0, kMaxUInt32, "unsigned long",
                        exception_state);
  }

  double number_value = number_object->Value();

  if (std::isnan(number_value))
    return 0;

  if (configuration == kClamp)
    return ClampTo<uint32_t>(number_value);

  if (std::isinf(number_value))
    return 0;

  uint32_t result;
  if (!number_object->Uint32Value(isolate->GetCurrentContext()).To(&result)) {
    exception_state.RethrowV8Exception(block.Exception());
    return 0;
  }
  return result;
}

int64_t ToInt64Slow(v8::Isolate* isolate,
                    v8::Local<v8::Value> value,
                    IntegerConversionConfiguration configuration,
                    ExceptionState& exception_state) {
  DCHECK(!value->IsInt32());

  v8::Local<v8::Number> number_object;
  // Can the value be converted to a number?
  v8::TryCatch block(isolate);
  if (!value->ToNumber(isolate->GetCurrentContext()).ToLocal(&number_object)) {
    exception_state.RethrowV8Exception(block.Exception());
    return 0;
  }
  DCHECK(!number_object.IsEmpty());

  double number_value = number_object->Value();

  if (configuration == kEnforceRange) {
    return EnforceRange(number_value, -kJSMaxInteger, kJSMaxInteger,
                        "long long", exception_state);
  }

  return DoubleToInteger(number_value);
}

uint64_t ToUInt64Slow(v8::Isolate* isolate,
                      v8::Local<v8::Value> value,
                      IntegerConversionConfiguration configuration,
                      ExceptionState& exception_state) {
  DCHECK(!value->IsUint32());
  if (value->IsInt32()) {
    DCHECK(configuration != kNormalConversion);
    int32_t result = value.As<v8::Int32>()->Value();
    if (result >= 0)
      return result;
    if (configuration == kEnforceRange) {
      exception_state.ThrowTypeError(
          "Value is outside the 'unsigned long long' value range.");
      return 0;
    }
    DCHECK_EQ(configuration, kClamp);
    return ClampTo<uint64_t>(result);
  }

  v8::Local<v8::Number> number_object;
  // Can the value be converted to a number?
  v8::TryCatch block(isolate);
  if (!value->ToNumber(isolate->GetCurrentContext()).ToLocal(&number_object)) {
    exception_state.RethrowV8Exception(block.Exception());
    return 0;
  }
  DCHECK(!number_object.IsEmpty());

  double number_value = number_object->Value();

  if (configuration == kEnforceRange) {
    return EnforceRange(number_value, 0, kJSMaxInteger, "unsigned long long",
                        exception_state);
  }

  if (std::isnan(number_value))
    return 0;

  if (configuration == kClamp)
    return ClampTo<uint64_t>(number_value);

  return DoubleToInteger(number_value);
}

float ToRestrictedFloat(v8::Isolate* isolate,
                        v8::Local<v8::Value> value,
                        ExceptionState& exception_state) {
  float number_value = ToFloat(isolate, value, exception_state);
  if (exception_state.HadException())
    return 0;
  if (!std::isfinite(number_value)) {
    exception_state.ThrowTypeError("The provided float value is non-finite.");
    return 0;
  }
  return number_value;
}

double ToDoubleSlow(v8::Isolate* isolate,
                    v8::Local<v8::Value> value,
                    ExceptionState& exception_state) {
  DCHECK(!value->IsNumber());
  v8::TryCatch block(isolate);
  v8::Local<v8::Number> number_value;
  if (!value->ToNumber(isolate->GetCurrentContext()).ToLocal(&number_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return 0;
  }
  return number_value->Value();
}

double ToRestrictedDouble(v8::Isolate* isolate,
                          v8::Local<v8::Value> value,
                          ExceptionState& exception_state) {
  double number_value = ToDouble(isolate, value, exception_state);
  if (exception_state.HadException())
    return 0;
  if (!std::isfinite(number_value)) {
    exception_state.ThrowTypeError("The provided double value is non-finite.");
    return 0;
  }
  return number_value;
}

static bool HasUnmatchedSurrogates(const String& string) {
  // By definition, 8-bit strings are confined to the Latin-1 code page and
  // have no surrogates, matched or otherwise.
  if (string.empty() || string.Is8Bit())
    return false;

  const UChar* characters = string.Characters16();
  const unsigned length = string.length();

  for (unsigned i = 0; i < length; ++i) {
    UChar c = characters[i];
    if (U16_IS_SINGLE(c))
      continue;
    if (U16_IS_TRAIL(c))
      return true;
    DCHECK(U16_IS_LEAD(c));
    if (i == length - 1)
      return true;
    UChar d = characters[i + 1];
    if (!U16_IS_TRAIL(d))
      return true;
    ++i;
  }
  return false;
}

// Replace unmatched surrogates with REPLACEMENT CHARACTER U+FFFD.
String ReplaceUnmatchedSurrogates(String string) {
  // This roughly implements https://webidl.spec.whatwg.org/#dfn-obtain-unicode
  // but since Blink strings are 16-bits internally, the output is simply
  // re-encoded to UTF-16.

  // The concept of surrogate pairs is explained at:
  // http://www.unicode.org/versions/Unicode6.2.0/ch03.pdf#G2630

  // Blink-specific optimization to avoid making an unnecessary copy.
  if (!HasUnmatchedSurrogates(string))
    return string;
  DCHECK(!string.Is8Bit());

  // 1. Let S be the DOMString value.
  const UChar* s = string.Characters16();

  // 2. Let n be the length of S.
  const unsigned n = string.length();

  // 3. Initialize i to 0.
  unsigned i = 0;

  // 4. Initialize U to be an empty sequence of Unicode characters.
  StringBuffer<UChar> result(n);
  UChar* u = result.Characters();

  // 5. While i < n:
  while (i < n) {
    // 1. Let c be the code unit in S at index i.
    UChar c = s[i];
    // 2. Depending on the value of c:
    if (U16_IS_SINGLE(c)) {
      // c < 0xD800 or c > 0xDFFF
      // Append to U the Unicode character with code point c.
      u[i] = c;
    } else if (U16_IS_TRAIL(c)) {
      // 0xDC00 <= c <= 0xDFFF
      // Append to U a U+FFFD REPLACEMENT CHARACTER.
      u[i] = kReplacementCharacter;
    } else {
      // 0xD800 <= c <= 0xDBFF
      DCHECK(U16_IS_LEAD(c));
      if (i == n - 1) {
        // 1. If i = n-1, then append to U a U+FFFD REPLACEMENT CHARACTER.
        u[i] = kReplacementCharacter;
      } else {
        // 2. Otherwise, i < n-1:
        DCHECK_LT(i, n - 1);
        // ....1. Let d be the code unit in S at index i+1.
        UChar d = s[i + 1];
        if (U16_IS_TRAIL(d)) {
          // 2. If 0xDC00 <= d <= 0xDFFF, then:
          // ..1. Let a be c & 0x3FF.
          // ..2. Let b be d & 0x3FF.
          // ..3. Append to U the Unicode character with code point
          //      2^16+2^10*a+b.
          u[i++] = c;
          u[i] = d;
        } else {
          // 3. Otherwise, d < 0xDC00 or d > 0xDFFF. Append to U a U+FFFD
          //    REPLACEMENT CHARACTER.
          u[i] = kReplacementCharacter;
        }
      }
    }
    // 3. Set i to i+1.
    ++i;
  }

  // 6. Return U.
  DCHECK_EQ(i, string.length());
  return String::Adopt(result);
}

DOMWindow* ToDOMWindow(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return V8Window::ToImplWithTypeCheck(isolate, value);
}

LocalDOMWindow* ToLocalDOMWindow(v8::Local<v8::Context> context) {
  if (context.IsEmpty())
    return nullptr;
  return To<LocalDOMWindow>(
      ToDOMWindow(context->GetIsolate(), context->Global()));
}

LocalDOMWindow* EnteredDOMWindow(v8::Isolate* isolate) {
  LocalDOMWindow* window =
      ToLocalDOMWindow(isolate->GetEnteredOrMicrotaskContext());
  DCHECK(window);
  return window;
}

LocalDOMWindow* IncumbentDOMWindow(v8::Isolate* isolate) {
  LocalDOMWindow* window = ToLocalDOMWindow(isolate->GetIncumbentContext());
  DCHECK(window);
  return window;
}

LocalDOMWindow* CurrentDOMWindow(v8::Isolate* isolate) {
  return ToLocalDOMWindow(isolate->GetCurrentContext());
}

ExecutionContext* ToExecutionContext(v8::Local<v8::Context> context) {
  // TODO(jgruber,crbug.com/v8/10460): Change this back to a DCHECK once the
  // crash has been flushed out.
  CHECK(!context.IsEmpty());

  RUNTIME_CALL_TIMER_SCOPE(context->GetIsolate(),
                           RuntimeCallStats::CounterId::kToExecutionContext);

  v8::Local<v8::Object> global_proxy = context->Global();

  // TODO(jgruber,crbug.com/v8/10460): Change these back to a DCHECK once the
  // crash has been flushed out.
  CHECK(!global_proxy.IsEmpty());
  CHECK(global_proxy->IsObject());

  // There are several contexts other than Window, WorkerGlobalScope or
  // WorkletGlobalScope but entering into ToExecutionContext, namely GC context,
  // DevTools' context (debug context), and maybe more.  They all don't have
  // any internal field.
  if (global_proxy->InternalFieldCount() == 0)
    return nullptr;

  ScriptWrappable::TypeDispatcher dispatcher(ToScriptWrappable(global_proxy));
  if (auto* x = dispatcher.ToMostDerived<DOMWindow>())
    return x->GetExecutionContext();
  if (auto* x = dispatcher.DowncastTo<WorkerGlobalScope>())
    return x->GetExecutionContext();
  if (auto* x = dispatcher.DowncastTo<WorkletGlobalScope>())
    return x->GetExecutionContext();
  if (auto* x = dispatcher.ToMostDerived<ShadowRealmGlobalScope>())
    return x->GetExecutionContext();

  NOTREACHED();
  return nullptr;
}

ExecutionContext* CurrentExecutionContext(v8::Isolate* isolate) {
  return ToExecutionContext(isolate->GetCurrentContext());
}

LocalFrame* ToLocalFrameIfNotDetached(v8::Local<v8::Context> context) {
  LocalDOMWindow* window = ToLocalDOMWindow(context);
  if (window && window->IsCurrentlyDisplayedInFrame())
    return window->GetFrame();
  // We return 0 here because |context| is detached from the Frame. If we
  // did return |frame| we could get in trouble because the frame could be
  // navigated to another security origin.
  return nullptr;
}

static ScriptState* ToScriptStateImpl(LocalFrame* frame,
                                      DOMWrapperWorld& world) {
  if (!frame)
    return nullptr;
  v8::Local<v8::Context> context = ToV8ContextEvenIfDetached(frame, world);
  if (context.IsEmpty())
    return nullptr;
  ScriptState* script_state = ScriptState::From(context);
  if (!script_state->ContextIsValid())
    return nullptr;
  DCHECK_EQ(frame, ToLocalFrameIfNotDetached(context));
  return script_state;
}

v8::Local<v8::Context> ToV8Context(ExecutionContext* context,
                                   DOMWrapperWorld& world) {
  DCHECK(context);
  if (LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(context)) {
    if (LocalFrame* frame = window->GetFrame())
      return ToV8Context(frame, world);
  } else if (auto* scope = DynamicTo<WorkerOrWorkletGlobalScope>(context)) {
    if (WorkerOrWorkletScriptController* script = scope->ScriptController()) {
      if (ScriptState* script_state = script->GetScriptState()) {
        if (script_state->ContextIsValid())
          return script_state->GetContext();
      }
    }
  }
  return v8::Local<v8::Context>();
}

v8::Local<v8::Context> ToV8Context(LocalFrame* frame, DOMWrapperWorld& world) {
  ScriptState* script_state = ToScriptStateImpl(frame, world);
  if (!script_state)
    return v8::Local<v8::Context>();
  return script_state->GetContext();
}

v8::Local<v8::Context> ToV8ContextEvenIfDetached(LocalFrame* frame,
                                                 DOMWrapperWorld& world) {
  // TODO(yukishiino): this method probably should not force context creation,
  // but it does through WindowProxy() call.
  DCHECK(frame);

  // TODO(crbug.com/1046282): The following bailout is a temporary fix
  // introduced due to crbug.com/1037985 .  Remove this temporary fix once
  // the root cause is fixed.
  if (frame->IsProvisional()) {
    base::debug::DumpWithoutCrashing();
    return v8::Local<v8::Context>();
  }

  return frame->WindowProxy(world)->ContextIfInitialized();
}

v8::Local<v8::Context> ToV8ContextMaybeEmpty(LocalFrame* frame,
                                             DOMWrapperWorld& world) {
  DCHECK(frame);

  // TODO(crbug.com/1046282): The following bailout is a temporary fix
  // introduced due to crbug.com/1037985 .  Remove this temporary fix once
  // the root cause is fixed.
  if (frame->IsProvisional()) {
    base::debug::DumpWithoutCrashing();
    return v8::Local<v8::Context>();
  }
  DCHECK(frame->WindowProxyMaybeUninitialized(world));
  v8::Local<v8::Context> context =
      frame->WindowProxyMaybeUninitialized(world)->ContextIfInitialized();

  DCHECK(context.IsEmpty() || frame == ToLocalFrameIfNotDetached(context));
  return context;
}

ScriptState* ToScriptState(ExecutionContext* context, DOMWrapperWorld& world) {
  DCHECK(context);
  if (LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(context)) {
    return ToScriptState(window->GetFrame(), world);
  } else if (auto* scope = DynamicTo<WorkerOrWorkletGlobalScope>(context)) {
    if (WorkerOrWorkletScriptController* script = scope->ScriptController()) {
      if (ScriptState* script_state = script->GetScriptState()) {
        if (script_state->ContextIsValid())
          return script_state;
      }
    }
  }
  return nullptr;
}

ScriptState* ToScriptState(LocalFrame* frame, DOMWrapperWorld& world) {
  if (!frame)
    return nullptr;
  v8::HandleScope handle_scope(ToIsolate(frame));
  return ToScriptStateImpl(frame, world);
}

ScriptState* ToScriptStateForMainWorld(LocalFrame* frame) {
  return ToScriptState(frame, DOMWrapperWorld::MainWorld());
}

bool IsValidEnum(const String& value,
                 const char* const* valid_values,
                 size_t length,
                 const String& enum_name,
                 ExceptionState& exception_state) {
  for (size_t i = 0; i < length; ++i) {
    // Avoid the strlen inside String::operator== (because of the StringView).
    if (WTF::Equal(value.Impl(), valid_values[i]))
      return true;
  }
  exception_state.ThrowTypeError("The provided value '" + value +
                                 "' is not a valid enum value of type " +
                                 enum_name + ".");
  return false;
}

bool IsValidEnum(const Vector<String>& values,
                 const char* const* valid_values,
                 size_t length,
                 const String& enum_name,
                 ExceptionState& exception_state) {
  for (auto value : values) {
    if (!IsValidEnum(value, valid_values, length, enum_name, exception_state))
      return false;
  }
  return true;
}

v8::Local<v8::Function> GetEsIteratorMethod(v8::Isolate* isolate,
                                            v8::Local<v8::Object> object,
                                            ExceptionState& exception_state) {
  const v8::Local<v8::Value> key = v8::Symbol::GetIterator(isolate);

  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Value> iterator_method;
  if (!object->Get(isolate->GetCurrentContext(), key)
           .ToLocal(&iterator_method)) {
    exception_state.RethrowV8Exception(try_catch.Exception());
    return v8::Local<v8::Function>();
  }

  if (iterator_method->IsNullOrUndefined())
    return v8::Local<v8::Function>();

  if (!iterator_method->IsFunction()) {
    exception_state.ThrowTypeError("Iterator must be callable function");
    return v8::Local<v8::Function>();
  }

  return iterator_method.As<v8::Function>();
}

v8::Local<v8::Object> GetEsIteratorWithMethod(
    v8::Isolate* isolate,
    v8::Local<v8::Function> getter_function,
    v8::Local<v8::Object> object,
    ExceptionState& exception_state) {
  v8::TryCatch block(isolate);
  v8::Local<v8::Value> iterator;
  if (!V8ScriptRunner::CallFunction(
           getter_function, ToExecutionContext(isolate->GetCurrentContext()),
           object, 0, nullptr, isolate)
           .ToLocal(&iterator)) {
    exception_state.RethrowV8Exception(block.Exception());
    return v8::Local<v8::Object>();
  }
  if (!iterator->IsObject()) {
    exception_state.ThrowTypeError("Iterator is not an object.");
    return v8::Local<v8::Object>();
  }
  return iterator.As<v8::Object>();
}

bool HasCallableIteratorSymbol(v8::Isolate* isolate,
                               v8::Local<v8::Value> value,
                               ExceptionState& exception_state) {
  if (!value->IsObject())
    return false;
  v8::Local<v8::Function> iterator_method =
      GetEsIteratorMethod(isolate, value.As<v8::Object>(), exception_state);
  return !iterator_method.IsEmpty();
}

v8::Isolate* ToIsolate(const LocalFrame* frame) {
  DCHECK(frame);
  return frame->GetWindowProxyManager()->GetIsolate();
}

v8::Local<v8::Value> FromJSONString(v8::Isolate* isolate,
                                    v8::Local<v8::Context> context,
                                    const String& stringified_json,
                                    ExceptionState& exception_state) {
  v8::Local<v8::Value> parsed;
  v8::TryCatch try_catch(isolate);
  if (!v8::JSON::Parse(context, V8String(isolate, stringified_json))
           .ToLocal(&parsed)) {
    if (try_catch.HasCaught())
      exception_state.RethrowV8Exception(try_catch.Exception());
  }

  return parsed;
}

Vector<String> GetOwnPropertyNames(v8::Isolate* isolate,
                                   const v8::Local<v8::Object>& object,
                                   ExceptionState& exception_state) {
  if (object.IsEmpty())
    return Vector<String>();

  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Array> property_names;
  if (!object->GetOwnPropertyNames(isolate->GetCurrentContext())
           .ToLocal(&property_names)) {
    exception_state.RethrowV8Exception(try_catch.Exception());
    return Vector<String>();
  }

  return NativeValueTraits<IDLSequence<IDLString>>::NativeValue(
      isolate, property_names, exception_state);
}

v8::MicrotaskQueue* ToMicrotaskQueue(ExecutionContext* execution_context) {
  if (!execution_context)
    return nullptr;
  return execution_context->GetMicrotaskQueue();
}

v8::MicrotaskQueue* ToMicrotaskQueue(ScriptState* script_state) {
  return ToMicrotaskQueue(ExecutionContext::From(script_state));
}

scheduler::EventLoop& ToEventLoop(ExecutionContext* execution_context) {
  DCHECK(execution_context);
  return *execution_context->GetAgent()->event_loop().get();
}

scheduler::EventLoop& ToEventLoop(ScriptState* script_state) {
  return ToEventLoop(ExecutionContext::From(script_state));
}

bool IsInParallelAlgorithmRunnable(ExecutionContext* execution_context,
                                   ScriptState* script_state) {
  if (!execution_context || execution_context->IsContextDestroyed())
    return false;

  // It's possible that execution_context is the one of the
  // document tree (i.e. the execution context of the document
  // that the receiver object currently belongs to) and
  // script_state is the one of the receiver object's creation
  // context (i.e. the script state of the V8 context in which
  // the receiver object was created). So, check the both contexts.
  // TODO(yukishiino): Find the necessary and sufficient conditions of the
  // runnability.
  if (!script_state->ContextIsValid())
    return false;

  return true;
}

}  // namespace blink
