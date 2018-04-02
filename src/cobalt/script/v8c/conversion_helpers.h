// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_V8C_CONVERSION_HELPERS_H_
#define COBALT_SCRIPT_V8C_CONVERSION_HELPERS_H_

#include <cmath>

#include <limits>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/optional.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "cobalt/base/compiler.h"
#include "cobalt/base/enable_if.h"
#include "cobalt/base/token.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/v8c/algorithm_helpers.h"
#include "cobalt/script/v8c/helpers.h"
#include "cobalt/script/v8c/type_traits.h"
#include "cobalt/script/v8c/union_type_conversion_forward.h"
#include "cobalt/script/v8c/v8c_callback_interface_holder.h"
#include "cobalt/script/v8c/v8c_global_environment.h"
#include "cobalt/script/v8c/v8c_user_object_holder.h"
#include "cobalt/script/v8c/v8c_value_handle.h"
#include "cobalt/script/value_handle.h"
#include "nb/memory_scope.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {

// Flags that can be used as a bitmask for special conversion behaviour.
enum ConversionFlags {
  kNoConversionFlags = 0,
  kConversionFlagRestricted = 1 << 0,
  kConversionFlagNullable = 1 << 1,
  kConversionFlagTreatNullAsEmptyString = 1 << 2,
  kConversionFlagTreatUndefinedAsEmptyString = 1 << 3,
  kConversionFlagClamped = 1 << 4,
  kConversionFlagObjectOnly = 1 << 5,

  // Valid conversion flags for numeric values.
  kConversionFlagsNumeric = kConversionFlagRestricted | kConversionFlagClamped,

  // Valid conversion flags for string types.
  kConversionFlagsString = kConversionFlagTreatNullAsEmptyString |
                           kConversionFlagTreatUndefinedAsEmptyString,

  // Valid conversion flags for objects.
  kConversionFlagsObject = kConversionFlagNullable,

  // Valid conversion flags for ValueHandles.
  kConversionFlagsValueHandle =
      kConversionFlagObjectOnly | kConversionFlagNullable,

  // Valid conversion flags for callback functions.
  kConversionFlagsCallbackFunction = kConversionFlagNullable,

  // Valid conversion flags for callback interfaces.
  kConversionFlagsCallbackInterface = kConversionFlagNullable,
};

// std::string -> JSValue
inline void ToJSValue(v8::Isolate* isolate, const std::string& in_string,
                      v8::Local<v8::Value>* out_value) {
  v8::MaybeLocal<v8::String> maybe_string = v8::String::NewFromUtf8(
      isolate, in_string.data(), v8::NewStringType::kNormal, in_string.size());
  v8::Local<v8::Value> string;
  if (!maybe_string.ToLocal(&string)) {
    *out_value = v8::Null(isolate);
    return;
  }

  *out_value = string;
}

// JSValue -> std::string
void FromJSValue(v8::Isolate* isolate, v8::Local<v8::Value> value,
                 int conversion_flags, ExceptionState* exception_state,
                 std::string* out_string);

// std::vector<uint8_t> -> JSValue
// Note that this conversion is specifically for the Web IDL type ByteString.
// Ideally, conversion requests would be explicit in what type they wanted to
// convert to, however it is currently solely based on input type.
inline void ToJSValue(v8::Isolate* isolate, const std::vector<uint8_t>& in_data,
                      v8::Local<v8::Value>* out_value) {
  v8::MaybeLocal<v8::String> maybe_string = v8::String::NewFromOneByte(
      isolate, in_data.data(), v8::NewStringType::kNormal, in_data.size());
  v8::Local<v8::Value> string;
  if (!maybe_string.ToLocal(&string)) {
    *out_value = v8::Null(isolate);
    return;
  }

  *out_value = string;
}

// base::Token -> JSValue
inline void ToJSValue(v8::Isolate* isolate, const base::Token& token,
                      v8::Local<v8::Value>* out_value) {
  ToJSValue(isolate, std::string(token.c_str()), out_value);
}

// bool -> JSValue
inline void ToJSValue(v8::Isolate* isolate, bool in_boolean,
                      v8::Local<v8::Value>* out_value) {
  *out_value = v8::Boolean::New(isolate, in_boolean);
}

// JSValue -> bool
inline void FromJSValue(v8::Isolate* isolate, v8::Local<v8::Value> value,
                        int conversion_flags, ExceptionState* exception_state,
                        bool* out_boolean) {
  DCHECK_EQ(conversion_flags, kNoConversionFlags)
      << "No conversion flags supported.";
  DCHECK(out_boolean);
  v8::MaybeLocal<v8::Boolean> maybe_boolean =
      value->ToBoolean(isolate->GetCurrentContext());
  v8::Local<v8::Boolean> boolean;
  if (!maybe_boolean.ToLocal(&boolean)) {
    // TODO: Handle this failure case.  It apparently can't happen in
    // SpiderMonkey.
    NOTIMPLEMENTED();
    return;
  }
  *out_boolean = boolean->Value();
}

// signed integers <= 4 bytes -> JSValue
template <typename T>
inline void ToJSValue(
    v8::Isolate* isolate, T in_number, v8::Local<v8::Value>* out_value,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer &&
                                 std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) <= 4),
                             T>::type* = NULL) {
  *out_value = v8::Integer::New(isolate, static_cast<int32_t>(in_number));
}

template <typename T>
inline const double UpperBound() {
  return std::numeric_limits<T>::max();
}

template <typename T>
inline const double LowerBound() {
  return std::numeric_limits<T>::min();
}

// The below specializations of UpperBound<T> and LowerBound<T> for 64
// bit integers use the (2^(53) - 1) and similar bounds specified in
// step 1 of ConvertToInt, see:
// https://heycam.github.io/webidl/#abstract-opdef-converttoint
template <>
inline const double UpperBound<int64_t>() {
  const double kInt64UpperBound = static_cast<double>((1ll << 53) - 1);
  return kInt64UpperBound;
}

template <>
inline const double LowerBound<int64_t>() {
  const double kInt64LowerBound = static_cast<double>(-(1ll << 53) + 1);
  return kInt64LowerBound;
}

template <>
inline const double UpperBound<uint64_t>() {
  const double kUInt64UpperBound = static_cast<double>((1ll << 53) - 1);
  return kUInt64UpperBound;
}

// TODO: Consider just having this return a value.  Or just inline in it now
// that we removed the code duplication across integer conversions.
template <typename T>
void ClampedValue(v8::Isolate* isolate, v8::Local<v8::Value> value,
                  v8::Local<v8::Value>* clamped_value) {
  v8::MaybeLocal<v8::Number> maybe_number =
      value->ToNumber(isolate->GetCurrentContext());
  v8::Local<v8::Number> number;
  if (!maybe_number.ToLocal(&number)) {
    return;
  }
  double value_of_number = number->Value();
  if (value_of_number > UpperBound<T>()) {
    value_of_number = UpperBound<T>();
  } else if (value_of_number < LowerBound<T>()) {
    value_of_number = LowerBound<T>();
  }
  *clamped_value = v8::Number::New(isolate, value_of_number);
}

namespace internal {

// A small helper metafunction for integer-like FromJSValue conversions to
// pick the right type to feed to V8, which can only be the output types
// observed here.
template <typename T>
struct IntegralTypeToOutType {
  static_assert(std::numeric_limits<T>::is_specialized &&
                    std::numeric_limits<T>::is_integer,
                "");
  using type = typename std::conditional<
      std::numeric_limits<T>::is_signed,
      typename std::conditional<(sizeof(T) <= 4), int32_t, int64_t>::type,
      typename std::conditional<(sizeof(T) <= 4), uint32_t,
                                uint64_t>::type>::type;
};

inline bool ToOutType(v8::Isolate* isolate, v8::Local<v8::Value> value,
                      int32_t* out) {
  v8::Maybe<int32_t> maybe_result =
      value->Int32Value(isolate->GetCurrentContext());
  if (!maybe_result.To(out)) {
    return false;
  }
  return true;
}

inline bool ToOutType(v8::Isolate* isolate, v8::Local<v8::Value> value,
                      int64_t* out) {
  v8::Maybe<int64_t> maybe_result =
      value->IntegerValue(isolate->GetCurrentContext());
  if (!maybe_result.To(out)) {
    return false;
  }
  *out = maybe_result.FromJust();
  return true;
}

inline bool ToOutType(v8::Isolate* isolate, v8::Local<v8::Value> value,
                      uint32_t* out) {
  v8::Maybe<uint32_t> maybe_result =
      value->Uint32Value(isolate->GetCurrentContext());
  if (!maybe_result.To(out)) {
    return false;
  }
  *out = maybe_result.FromJust();
  return true;
}

inline bool ToOutType(v8::Isolate* isolate, v8::Local<v8::Value> value,
                      uint64_t* out) {
  // V8 doesn't have anything for uint64_t (TODO: why?  More spec compliant?
  // less spec compliant?...), so this one is different.
  v8::Maybe<double> maybe_result =
      value->NumberValue(isolate->GetCurrentContext());
  if (maybe_result.IsNothing()) {
    return false;
  }
  *out = static_cast<uint64_t>(maybe_result.FromJust());
  return true;
}

}  // namespace internal

// JSValue -> integer-like
template <typename T>
inline void FromJSValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, int conversion_flags,
    ExceptionState* exception_state, T* out_number,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer,
                             T>::type* = NULL) {
  DCHECK(out_number);

  if (UNLIKELY(value->IsSymbol())) {
    exception_state->SetSimpleException(
        kTypeError, "Cannot convert a Symbol value to a number");
    return;
  }

  // Convert a JavaScript value to an integer type as specified by the
  // ECMAScript standard.
  using OutType = typename internal::IntegralTypeToOutType<T>::type;
  OutType out;
  bool success;
  if (UNLIKELY(conversion_flags & kConversionFlagClamped)) {
    v8::Local<v8::Value> clamped_value;
    ClampedValue<T>(isolate, value, &clamped_value);
    DCHECK(!clamped_value.IsEmpty());
    success = internal::ToOutType(isolate, clamped_value, &out);
  } else {
    success = internal::ToOutType(isolate, value, &out);
  }

  // It is possible for |JS::To{Uint,Int}{32,64}| to fail in certain edge
  // cases, such as application JavaScript setting up infinite recursion that
  // gets triggered by the conversion.
  if (!success) {
    // TODO: Still need to handle infinite recursion edge case here, see
    // mozjs-45 version of this function.
    exception_state->SetSimpleException(
        std::numeric_limits<T>::is_signed ? kNotInt64Type : kNotUint64Type);
    return;
  }
  *out_number = static_cast<T>(out);
}

// signed integers > 4 bytes -> JSValue
template <typename T>
inline void ToJSValue(
    v8::Isolate* isolate, T in_number, v8::Local<v8::Value>* out_value,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer &&
                                 std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) > 4),
                             T>::type* = NULL) {
  *out_value = v8::Number::New(isolate, in_number);
}

// unsigned integers <= 4 bytes -> JSValue
template <typename T>
inline void ToJSValue(
    v8::Isolate* isolate, T in_number, v8::Local<v8::Value>* out_value,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer &&
                                 !std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) <= 4),
                             T>::type* = NULL) {
  *out_value = v8::Integer::NewFromUnsigned(isolate, in_number);
}

// TODO: Don't specialize template on these..., have them share an impl w/
// double unsigned integers > 4 bytes -> JSValue
template <typename T>
inline void ToJSValue(
    v8::Isolate* isolate, T in_number, v8::Local<v8::Value>* out_value,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 std::numeric_limits<T>::is_integer &&
                                 !std::numeric_limits<T>::is_signed &&
                                 (sizeof(T) > 4),
                             T>::type* = NULL) {
  *out_value = v8::Number::New(isolate, in_number);
}

// double -> JSValue
template <typename T>
inline void ToJSValue(
    v8::Isolate* isolate, T in_number, v8::Local<v8::Value>* out_value,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 !std::numeric_limits<T>::is_integer,
                             T>::type* = NULL) {
  *out_value = v8::Number::New(isolate, in_number);
}

// JSValue -> double
template <typename T>
inline void FromJSValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, int conversion_flags,
    ExceptionState* exception_state, T* out_number,
    typename base::enable_if<std::numeric_limits<T>::is_specialized &&
                                 !std::numeric_limits<T>::is_integer,
                             T>::type* = NULL) {
  DCHECK_EQ(conversion_flags & ~kConversionFlagsNumeric, 0)
      << "Unexpected conversion flags found.";

  v8::MaybeLocal<v8::Number> maybe_value_as_number =
      value->ToNumber(isolate->GetCurrentContext());
  v8::Local<v8::Number> value_as_number;
  if (!maybe_value_as_number.ToLocal(&value_as_number)) {
    NOTIMPLEMENTED();
    return;
  }
  double value_as_double = value_as_number->Value();
  if ((conversion_flags & kConversionFlagRestricted) &&
      !std::isfinite(value_as_double)) {
    exception_state->SetSimpleException(kNotFinite);
    return;
  }
  *out_number = value_as_double;
}

// optional<T> -> JSValue
template <typename T>
inline void ToJSValue(v8::Isolate* isolate,
                      const base::optional<T>& in_optional,
                      v8::Local<v8::Value>* out_value) {
  if (in_optional) {
    ToJSValue(isolate, in_optional.value(), out_value);
  } else {
    *out_value = v8::Null(isolate);
  }
}

// JSValue -> optional<T>
template <typename T>
inline void FromJSValue(v8::Isolate* isolate, v8::Local<v8::Value> value,
                        int conversion_flags, ExceptionState* exception_state,
                        base::optional<T>* out_optional) {
  if (value->IsNullOrUndefined()) {
    *out_optional = base::nullopt;
  } else {
    *out_optional = T();
    FromJSValue(isolate, value, conversion_flags & ~kConversionFlagNullable,
                exception_state, &(out_optional->value()));
  }
}

// JSValue -> optional<std::string>
template <>
inline void FromJSValue(v8::Isolate* isolate, v8::Local<v8::Value> value,
                        int conversion_flags, ExceptionState* exception_state,
                        base::optional<std::string>* out_optional) {
  if (value->IsNull()) {
    *out_optional = base::nullopt;
  } else if (value->IsUndefined() &&
             !(conversion_flags & kConversionFlagTreatUndefinedAsEmptyString)) {
    // If TreatUndefinedAs=EmptyString is set, skip the default conversion
    // of undefined to null.
    *out_optional = base::nullopt;
  } else {
    *out_optional = std::string();
    FromJSValue(isolate, value, conversion_flags & ~kConversionFlagNullable,
                exception_state, &(out_optional->value()));
  }
}

// ValueHandle -> JSValue
void ToJSValue(v8::Isolate* isolate,
               const ValueHandleHolder* value_handle_holder,
               v8::Local<v8::Value>* out_value);

// base::Time -> JSValue
void ToJSValue(v8::Isolate* isolate, const base::Time& time,
               v8::Local<v8::Value>* out_value);

// JSValue -> base::Time
void FromJSValue(v8::Isolate* isolate, v8::Local<v8::Value> value,
                 int conversion_flags, ExceptionState* exception_state,
                 base::Time* out_time);

// JSValue -> ValueHandle
void FromJSValue(v8::Isolate* isolate, v8::Local<v8::Value> value,
                 int conversion_flags, ExceptionState* exception_state,
                 V8cValueHandleHolder* out_holder);

// object -> JSValue
template <typename T>
inline void ToJSValue(v8::Isolate* isolate, const scoped_refptr<T>& in_object,
                      v8::Local<v8::Value>* out_value) {
  if (!in_object) {
    *out_value = v8::Null(isolate);
    return;
  }

  V8cGlobalEnvironment* global_environment =
      V8cGlobalEnvironment::GetFromIsolate(isolate);
  *out_value = global_environment->wrapper_factory()->GetWrapper(in_object);
}

// raw object pointer -> JSValue
template <typename T>
inline void ToJSValue(v8::Isolate* isolate, T* in_object,
                      v8::Local<v8::Value>* out_value) {
  ToJSValue(isolate, scoped_refptr<T>(in_object), out_value);
}

// JSValue -> object
template <typename T>
inline void FromJSValue(v8::Isolate* isolate, v8::Local<v8::Value> value,
                        int conversion_flags, ExceptionState* exception_state,
                        scoped_refptr<T>* out_object) {
  DCHECK_EQ(conversion_flags & ~kConversionFlagsObject, 0)
      << "Unexpected conversion flags found.";

  if (value->IsNullOrUndefined()) {
    if (!(conversion_flags & kConversionFlagNullable)) {
      exception_state->SetSimpleException(kNotNullableType);
    }
    return;
  }
  if (!value->IsObject()) {
    exception_state->SetSimpleException(kNotObjectType);
    return;
  }

  V8cGlobalEnvironment* global_environment =
      V8cGlobalEnvironment::GetFromIsolate(isolate);
  WrapperFactory* wrapper_factory = global_environment->wrapper_factory();
  v8::Local<v8::Object> object = value.As<v8::Object>();

  // Note that |DoesObjectImplementInterface| handles the case in which
  // |object| has no |WrapperPrivate|.
  if (!wrapper_factory->DoesObjectImplementInterface(object,
                                                     base::GetTypeId<T>())) {
    exception_state->SetSimpleException(kDoesNotImplementInterface);
    return;
  }

  WrapperPrivate* wrapper_private =
      WrapperPrivate::GetFromWrapperObject(object);
  DCHECK(wrapper_private);
  *out_object = wrapper_private->wrappable<T>();
}

// CallbackInterface -> JSValue
template <typename T>
inline void ToJSValue(v8::Isolate* isolate,
                      const ScriptValue<T>* callback_interface,
                      v8::Local<v8::Value>* out_value) {
  if (!callback_interface) {
    *out_value = v8::Null(isolate);
    return;
  }

  typedef typename CallbackInterfaceTraits<T>::V8cCallbackInterfaceClass
      V8cCallbackInterfaceClass;
  // Downcast to V8cUserObjectHolder<T> so we can get the underlying JSObject.
  typedef V8cUserObjectHolder<V8cCallbackInterfaceClass>
      V8cUserObjectHolderClass;
  const V8cUserObjectHolderClass* user_object_holder =
      base::polymorphic_downcast<const V8cUserObjectHolderClass*>(
          callback_interface);

  // Shouldn't be NULL. If the callback was NULL then NULL should have been
  // passed as an argument into this function.
  // Downcast to the corresponding V8cCallbackInterface type, from which we
  // can get the implementing object.
  const V8cCallbackInterfaceClass* v8c_callback_interface =
      base::polymorphic_downcast<const V8cCallbackInterfaceClass*>(
          user_object_holder->GetValue());
  DCHECK(v8c_callback_interface);
  *out_value = v8c_callback_interface->NewLocal(isolate);
}

// JSValue -> CallbackInterface
template <typename T>
inline void FromJSValue(v8::Isolate* isolate, v8::Local<v8::Value> value,
                        int conversion_flags, ExceptionState* out_exception,
                        V8cCallbackInterfaceHolder<T>* out_callback_interface) {
  typedef T V8cCallbackInterfaceClass;
  DCHECK_EQ(conversion_flags & ~kConversionFlagsCallbackFunction, 0)
      << "No conversion flags supported.";
  if (value->IsNull()) {
    if (!(conversion_flags & kConversionFlagNullable)) {
      out_exception->SetSimpleException(kNotNullableType);
    }
    // If it is a nullable type, just return.
    return;
  }

  // https://www.w3.org/TR/WebIDL/#es-user-objects
  // Any user object can be considered to implement a user interface. Actually
  // checking if the correct properties exist will happen when the operation
  // on the callback interface is run.
  if (!value->IsObject()) {
    out_exception->SetSimpleException(kNotObjectType);
    return;
  }

  *out_callback_interface = V8cCallbackInterfaceHolder<T>(isolate, value);
}

// Sequence -> JSValue
template <typename T>
void ToJSValue(v8::Isolate* isolate, const script::Sequence<T>& sequence,
               v8::Local<v8::Value>* out_value) {
  // 1. Let n be the length of S.
  using size_type = typename script::Sequence<T>::size_type;
  size_type count = sequence.size();

  // 2. Let A be a new Array object created as if by the expression [].
  v8::Local<v8::Array> array = v8::Array::New(isolate);

  // 3. Initialize i to be 0.
  // 4. While i < n:
  for (size_type index = 0; index < count; ++index) {
    // 4.1. Let V be the value in S at index i.
    // 4.2. Let E be the result of converting V to an ECMAScript value.
    v8::Local<v8::Value> element;
    ToJSValue(isolate, sequence.at(index), &element);

    // 4.3. Let P be the result of calling ToString(i).
    // 4.4. Call CreateDataProperty(A, P, E).
    v8::Maybe<bool> set_result =
        array->Set(isolate->GetCurrentContext(), index, element);

    // 4.5. Set i to i + 1.
  }

  // 5. Return A.
  *out_value = array;
}

// JSValue -> Sequence
template <typename T>
void FromJSValue(v8::Isolate* isolate, v8::Local<v8::Value> value,
                 int conversion_flags, ExceptionState* exception_state,
                 script::Sequence<T>* out_sequence) {
  DCHECK_EQ(0, conversion_flags);
  DCHECK(out_sequence);

  // JS -> IDL type conversion procedure described here:
  // https://heycam.github.io/webidl/#es-sequence

  // 1. If Type(V) is not Object, throw a TypeError.
  if (!value->IsObject()) {
    exception_state->SetSimpleException(kNotObjectType);
    return;
  }

  // 2. Let method be ? GetMethod(V, @@iterator).
  // 3. If method is undefined, throw a TypeError.
  v8::Local<v8::Object> iterable = value.As<v8::Object>();

  v8::Local<v8::Object> iterator;
  V8cExceptionState* v8c_exception_state =
      base::polymorphic_downcast<V8cExceptionState*>(exception_state);
  if (!GetIterator(isolate, iterable, v8c_exception_state).ToLocal(&iterator)) {
    DCHECK(v8c_exception_state->is_exception_set());
    return;
  }

  v8::Local<v8::String> next_key = NewInternalString(isolate, "next");
  v8::Local<v8::String> value_key = NewInternalString(isolate, "value");
  v8::Local<v8::String> done_key = NewInternalString(isolate, "done");
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  // 4. Return the result of creating a sequence from V and method.
  // https://heycam.github.io/webidl/#create-sequence-from-iterable
  for (;;) {
    // Let next be ? IteratorStep(iter).
    v8::Local<v8::Value> next;
    if (!iterator->Get(context, next_key).ToLocal(&next)) {
      exception_state->SetSimpleException(kTypeError, "");
      return;
    }
    if (!next->IsFunction()) {
      exception_state->SetSimpleException(kTypeError,
                                          "Iterator.next should be callable.");
      return;
    }

    v8::Local<v8::Value> next_result;
    if (!next.As<v8::Function>()
             ->Call(context, iterator, 0, nullptr)
             .ToLocal(&next_result)) {
      return;
    }
    if (!next_result->IsObject()) {
      exception_state->SetSimpleException(
          kTypeError, "Iterator.next did not return an object.");
      return;
    }

    // Let nextItem be ? IteratorValue(next).
    v8::Local<v8::Object> result_object = next_result.As<v8::Object>();
    v8::Local<v8::Value> next_item;
    v8::Local<v8::Value> done;
    if (!result_object->Get(context, value_key).ToLocal(&next_item) ||
        !result_object->Get(context, done_key).ToLocal(&done)) {
      return;
    }

    bool done_as_bool;
    if (!done->BooleanValue(context).To(&done_as_bool)) {
      return;
    }
    if (done_as_bool) {
      break;
    }

    // Initialize S_i to the result of converting nextItem to an IDL value
    // of type T.
    T idl_next_item;
    FromJSValue(isolate, next_item, conversion_flags, exception_state,
                &idl_next_item);
    if (v8c_exception_state->is_exception_set()) {
      return;
    }
    out_sequence->push_back(idl_next_item);
  }
}

template <typename T>
void ToJSValue(v8::Isolate* isolate,
               const ScriptValue<Promise<T>>* promise_holder,
               v8::Local<v8::Value>* out_value);

template <typename T>
void ToJSValue(v8::Isolate* isolate, ScriptValue<Promise<T>>* promise_holder,
               v8::Local<v8::Value>* out_value);

// script::Handle<T> -> JSValue
template <typename T>
void ToJSValue(v8::Isolate* isolate, const Handle<T>& local,
               v8::Local<v8::Value>* out_value) {
  TRACK_MEMORY_SCOPE("Javascript");
  ToJSValue(isolate, local.GetScriptValue(), out_value);
}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

// Union type conversion is generated by a pump script.
#include "cobalt/script/v8c/union_type_conversion_impl.h"

#endif  // COBALT_SCRIPT_V8C_CONVERSION_HELPERS_H_
