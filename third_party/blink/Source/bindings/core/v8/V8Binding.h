/*
* Copyright (C) 2009 Google Inc. All rights reserved.
* Copyright (C) 2012 Ericsson AB. All rights reserved.
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

#ifndef V8Binding_h
#define V8Binding_h

#include "bindings/core/v8/DOMDataStore.h"
#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "bindings/core/v8/V8StringResource.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "bindings/core/v8/V8ValueCache.h"
#include "platform/heap/Heap.h"
#include "wtf/GetPtr.h"
#include "wtf/text/AtomicString.h"
#include <v8.h>

namespace blink {

class LocalDOMWindow;
class Document;
class EventListener;
class ExecutionContext;
class ExceptionState;
class LocalFrame;
class NodeFilter;
class XPathNSResolver;

namespace TraceEvent {
class ConvertableToTraceFormat;
}

const int kMaxRecursionDepth = 22;

// Helpers for throwing JavaScript TypeErrors for arity mismatches.
void setArityTypeError(ExceptionState&, const char* valid, unsigned provided);
v8::Local<v8::Value> createMinimumArityTypeErrorForMethod(v8::Isolate*, const char* method, const char* type, unsigned expected, unsigned provided);
v8::Local<v8::Value> createMinimumArityTypeErrorForConstructor(v8::Isolate*, const char* type, unsigned expected, unsigned provided);
void setMinimumArityTypeError(ExceptionState&, unsigned expected, unsigned provided);

v8::ArrayBuffer::Allocator* v8ArrayBufferAllocator();

template<typename CallbackInfo, typename S>
inline void v8SetReturnValue(const CallbackInfo& info, const v8::Persistent<S>& handle)
{
    info.GetReturnValue().Set(handle);
}

template<typename CallbackInfo, typename S>
inline void v8SetReturnValue(const CallbackInfo& info, const v8::Handle<S> handle)
{
    info.GetReturnValue().Set(handle);
}

template<typename CallbackInfo>
inline void v8SetReturnValue(const CallbackInfo& info, bool value)
{
    info.GetReturnValue().Set(value);
}

template<typename CallbackInfo>
inline void v8SetReturnValue(const CallbackInfo& info, double value)
{
    info.GetReturnValue().Set(value);
}

template<typename CallbackInfo>
inline void v8SetReturnValue(const CallbackInfo& info, int32_t value)
{
    info.GetReturnValue().Set(value);
}

template<typename CallbackInfo>
inline void v8SetReturnValue(const CallbackInfo& info, uint32_t value)
{
    info.GetReturnValue().Set(value);
}

template<typename CallbackInfo>
inline void v8SetReturnValueBool(const CallbackInfo& info, bool v)
{
    info.GetReturnValue().Set(v);
}

template<typename CallbackInfo>
inline void v8SetReturnValueInt(const CallbackInfo& info, int v)
{
    info.GetReturnValue().Set(v);
}

template<typename CallbackInfo>
inline void v8SetReturnValueUnsigned(const CallbackInfo& info, unsigned v)
{
    info.GetReturnValue().Set(v);
}

template<typename CallbackInfo>
inline void v8SetReturnValueNull(const CallbackInfo& info)
{
    info.GetReturnValue().SetNull();
}

template<typename CallbackInfo>
inline void v8SetReturnValueUndefined(const CallbackInfo& info)
{
    info.GetReturnValue().SetUndefined();
}

template<typename CallbackInfo>
inline void v8SetReturnValueEmptyString(const CallbackInfo& info)
{
    info.GetReturnValue().SetEmptyString();
}

template<typename CallbackInfo>
inline void v8SetReturnValueString(const CallbackInfo& info, const String& string, v8::Isolate* isolate)
{
    if (string.isNull()) {
        v8SetReturnValueEmptyString(info);
        return;
    }
    V8PerIsolateData::from(isolate)->stringCache()->setReturnValueFromString(info.GetReturnValue(), string.impl());
}

template<typename CallbackInfo>
inline void v8SetReturnValueStringOrNull(const CallbackInfo& info, const String& string, v8::Isolate* isolate)
{
    if (string.isNull()) {
        v8SetReturnValueNull(info);
        return;
    }
    V8PerIsolateData::from(isolate)->stringCache()->setReturnValueFromString(info.GetReturnValue(), string.impl());
}

template<typename CallbackInfo>
inline void v8SetReturnValueStringOrUndefined(const CallbackInfo& info, const String& string, v8::Isolate* isolate)
{
    if (string.isNull()) {
        v8SetReturnValueUndefined(info);
        return;
    }
    V8PerIsolateData::from(isolate)->stringCache()->setReturnValueFromString(info.GetReturnValue(), string.impl());
}

template<typename CallbackInfo>
inline void v8SetReturnValue(const CallbackInfo& callbackInfo, ScriptWrappable* impl)
{
    if (UNLIKELY(!impl)) {
        v8SetReturnValueNull(callbackInfo);
        return;
    }
    if (DOMDataStore::setReturnValueNonTemplate(callbackInfo.GetReturnValue(), impl))
        return;
    v8::Handle<v8::Object> wrapper = impl->wrap(callbackInfo.Holder(), callbackInfo.GetIsolate());
    v8SetReturnValue(callbackInfo, wrapper);
}

template<typename CallbackInfo>
inline void v8SetReturnValue(const CallbackInfo& callbackInfo, Node* impl)
{
    if (UNLIKELY(!impl)) {
        v8SetReturnValueNull(callbackInfo);
        return;
    }
    if (DOMDataStore::setReturnValueNonTemplate(callbackInfo.GetReturnValue(), impl))
        return;
    v8::Handle<v8::Object> wrapper = ScriptWrappable::fromNode(impl)->wrap(callbackInfo.Holder(), callbackInfo.GetIsolate());
    v8SetReturnValue(callbackInfo, wrapper);
}

template<typename CallbackInfo, typename T>
inline void v8SetReturnValue(const CallbackInfo& callbackInfo, PassRefPtr<T> impl)
{
    v8SetReturnValue(callbackInfo, impl.get());
}

template<typename CallbackInfo, typename T>
inline void v8SetReturnValue(const CallbackInfo& callbackInfo, RawPtr<T> impl)
{
    v8SetReturnValue(callbackInfo, impl.get());
}

template<typename CallbackInfo>
inline void v8SetReturnValueForMainWorld(const CallbackInfo& callbackInfo, ScriptWrappable* impl)
{
    ASSERT(DOMWrapperWorld::current(callbackInfo.GetIsolate()).isMainWorld());
    if (UNLIKELY(!impl)) {
        v8SetReturnValueNull(callbackInfo);
        return;
    }
    if (DOMDataStore::setReturnValueForMainWorldNonTemplate(callbackInfo.GetReturnValue(), impl))
        return;
    v8::Handle<v8::Object> wrapper = impl->wrap(callbackInfo.Holder(), callbackInfo.GetIsolate());
    v8SetReturnValue(callbackInfo, wrapper);
}

template<typename CallbackInfo, typename T>
inline void v8SetReturnValueForMainWorld(const CallbackInfo& callbackInfo, PassRefPtr<T> impl)
{
    v8SetReturnValueForMainWorld(callbackInfo, impl.get());
}

template<typename CallbackInfo, typename T>
inline void v8SetReturnValueForMainWorld(const CallbackInfo& callbackInfo, RawPtr<T> impl)
{
    v8SetReturnValueForMainWorld(callbackInfo, impl.get());
}

template<typename CallbackInfo>
inline void v8SetReturnValueFast(const CallbackInfo& callbackInfo, ScriptWrappable* impl, const ScriptWrappable* wrappable)
{
    if (UNLIKELY(!impl)) {
        v8SetReturnValueNull(callbackInfo);
        return;
    }
    if (DOMDataStore::setReturnValueFastNonTemplate(callbackInfo.GetReturnValue(), impl, callbackInfo.Holder(), wrappable))
        return;
    v8::Handle<v8::Object> wrapper = impl->wrap(callbackInfo.Holder(), callbackInfo.GetIsolate());
    v8SetReturnValue(callbackInfo, wrapper);
}

template<typename CallbackInfo>
inline void v8SetReturnValueFast(const CallbackInfo& callbackInfo, Node* impl, const ScriptWrappable* wrappable)
{
    if (UNLIKELY(!impl)) {
        v8SetReturnValueNull(callbackInfo);
        return;
    }
    if (DOMDataStore::setReturnValueFastNonTemplate(callbackInfo.GetReturnValue(), impl, callbackInfo.Holder(), wrappable))
        return;
    v8::Handle<v8::Object> wrapper = ScriptWrappable::fromNode(impl)->wrap(callbackInfo.Holder(), callbackInfo.GetIsolate());
    v8SetReturnValue(callbackInfo, wrapper);
}

template<typename CallbackInfo>
inline void v8SetReturnValueFast(const CallbackInfo& callbackInfo, ScriptWrappable* impl, const ScriptWrappableBase*)
{
    // If the third arg is not ScriptWrappable, there is no fast path.
    v8SetReturnValue(callbackInfo, impl);
}

template<typename CallbackInfo, typename T, typename Wrappable>
inline void v8SetReturnValueFast(const CallbackInfo& callbackInfo, PassRefPtr<T> impl, const Wrappable* wrappable)
{
    v8SetReturnValueFast(callbackInfo, impl.get(), wrappable);
}

template<typename CallbackInfo, typename T, typename Wrappable>
inline void v8SetReturnValueFast(const CallbackInfo& callbackInfo, RawPtr<T> impl, const Wrappable* wrappable)
{
    v8SetReturnValueFast(callbackInfo, impl.get(), wrappable);
}

// Convert v8::String to a WTF::String. If the V8 string is not already
// an external string then it is transformed into an external string at this
// point to avoid repeated conversions.
inline String toCoreString(v8::Handle<v8::String> value)
{
    return v8StringToWebCoreString<String>(value, Externalize);
}

inline String toCoreStringWithNullCheck(v8::Handle<v8::String> value)
{
    if (value.IsEmpty() || value->IsNull())
        return String();
    return toCoreString(value);
}

inline String toCoreStringWithUndefinedOrNullCheck(v8::Handle<v8::String> value)
{
    if (value.IsEmpty() || value->IsNull() || value->IsUndefined())
        return String();
    return toCoreString(value);
}

inline AtomicString toCoreAtomicString(v8::Handle<v8::String> value)
{
    return v8StringToWebCoreString<AtomicString>(value, Externalize);
}

// This method will return a null String if the v8::Value does not contain a v8::String.
// It will not call ToString() on the v8::Value. If you want ToString() to be called,
// please use the TONATIVE_FOR_V8STRINGRESOURCE_*() macros instead.
inline String toCoreStringWithUndefinedOrNullCheck(v8::Handle<v8::Value> value)
{
    if (value.IsEmpty() || !value->IsString())
        return String();
    return toCoreString(value.As<v8::String>());
}

// Convert a string to a V8 string.
// Return a V8 external string that shares the underlying buffer with the given
// WebCore string. The reference counting mechanism is used to keep the
// underlying buffer alive while the string is still live in the V8 engine.
inline v8::Handle<v8::String> v8String(v8::Isolate* isolate, const String& string)
{
    if (string.isNull())
        return v8::String::Empty(isolate);
    return V8PerIsolateData::from(isolate)->stringCache()->v8ExternalString(string.impl(), isolate);
}

inline v8::Handle<v8::String> v8AtomicString(v8::Isolate* isolate, const char* str)
{
    ASSERT(isolate);
    return v8::String::NewFromUtf8(isolate, str, v8::String::kInternalizedString, strlen(str));
}

inline v8::Handle<v8::String> v8AtomicString(v8::Isolate* isolate, const char* str, size_t length)
{
    ASSERT(isolate);
    return v8::String::NewFromUtf8(isolate, str, v8::String::kInternalizedString, length);
}

inline v8::Handle<v8::Value> v8Undefined()
{
    return v8::Handle<v8::Value>();
}

inline v8::Handle<v8::Value> toV8(ScriptWrappable* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    if (UNLIKELY(!impl))
        return v8::Null(isolate);
    v8::Handle<v8::Value> wrapper = DOMDataStore::getWrapperNonTemplate(impl, isolate);
    if (!wrapper.IsEmpty())
        return wrapper;

    return impl->wrap(creationContext, isolate);
}

inline v8::Handle<v8::Value> toV8(Node* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    if (UNLIKELY(!impl))
        return v8::Null(isolate);
    v8::Handle<v8::Value> wrapper = DOMDataStore::getWrapperNonTemplate(impl, isolate);
    if (!wrapper.IsEmpty())
        return wrapper;

    return ScriptWrappable::fromNode(impl)->wrap(creationContext, isolate);
}

template<typename T>
inline v8::Handle<v8::Value> toV8(RefPtr<T>& impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return toV8(impl.get(), creationContext, isolate);
}

template<typename T>
inline v8::Handle<v8::Value> toV8(PassRefPtr<T> impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return toV8(impl.get(), creationContext, isolate);
}

template<typename T>
inline v8::Handle<v8::Value> toV8(RawPtr<T> impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return toV8(impl.get(), creationContext, isolate);
}

// Converts a DOM object to a v8 value. This function is intended to be used
// internally. If you want to convert a DOM object to a V8 value,
//  - Use toV8 if you can include V8X.h.
//  - Use V8ValueTraits<T>::toV8Value if you cannot include V8X.h.
// Note: Including bindings/{core, modules}/v8/V8*.h from core and modules
// is fine. In such a case, perhaps toV8 is what you want.
// Note: toV8NoInline is a non-inline toV8 and V8ValueTraits::toV8Value offers
// more: You can handle value conversion generally with it.
template<typename T>
v8::Handle<v8::Value> toV8NoInline(T* impl, v8::Handle<v8::Object> creationContext, v8::Isolate*);

template <typename T>
struct V8ValueTraits {
    static v8::Handle<v8::Value> toV8Value(const T& value, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
    {
        if (!WTF::getPtr(value))
            return v8::Null(isolate);
        return toV8NoInline(WTF::getPtr(value), creationContext, isolate);
    }
};

template<>
struct V8ValueTraits<String> {
    static inline v8::Handle<v8::Value> toV8Value(const String& value, v8::Handle<v8::Object>, v8::Isolate* isolate)
    {
        return v8String(isolate, value);
    }
};

template<>
struct V8ValueTraits<AtomicString> {
    static inline v8::Handle<v8::Value> toV8Value(const AtomicString& value, v8::Handle<v8::Object>, v8::Isolate* isolate)
    {
        return v8String(isolate, value);
    }
};

template<size_t n>
struct V8ValueTraits<char[n]> {
    static inline v8::Handle<v8::Value> toV8Value(char const (&value)[n], v8::Handle<v8::Object>, v8::Isolate* isolate)
    {
        return v8String(isolate, value);
    }
};

template<size_t n>
struct V8ValueTraits<char const[n]> {
    static inline v8::Handle<v8::Value> toV8Value(char const (&value)[n], v8::Handle<v8::Object>, v8::Isolate* isolate)
    {
        return v8String(isolate, value);
    }
};

template<>
struct V8ValueTraits<const char*> {
    static inline v8::Handle<v8::Value> toV8Value(const char* const& value, v8::Handle<v8::Object>, v8::Isolate* isolate)
    {
        if (!value) {
            // We return an empty string, not null, in order to align
            // with v8String(isolate, String()).
            return v8::String::Empty(isolate);
        }
        return v8String(isolate, value);
    }
};

template<>
struct V8ValueTraits<char*> {
    static inline v8::Handle<v8::Value> toV8Value(char* const& value, v8::Handle<v8::Object> object, v8::Isolate* isolate)
    {
        return V8ValueTraits<const char*>::toV8Value(value, object, isolate);
    }
};

template<>
struct V8ValueTraits<std::nullptr_t> {
    static inline v8::Handle<v8::Value> toV8Value(std::nullptr_t null, v8::Handle<v8::Object> object, v8::Isolate* isolate)
    {
        // Note: toV8Value(nullptr) returns null and
        // toV8Value(static_cast<const char*>(nullptr)) returns an empty string.
        // See V8ValueTraits<const char*>.
        return v8::Null(isolate);
    }
};

template<>
struct V8ValueTraits<int> {
    static inline v8::Handle<v8::Value> toV8Value(const int& value, v8::Handle<v8::Object>, v8::Isolate* isolate)
    {
        return v8::Integer::New(isolate, value);
    }
};

template<>
struct V8ValueTraits<long> {
    static inline v8::Handle<v8::Value> toV8Value(const long& value, v8::Handle<v8::Object>, v8::Isolate* isolate)
    {
        return v8::Integer::New(isolate, value);
    }
};

template<>
struct V8ValueTraits<unsigned> {
    static inline v8::Handle<v8::Value> toV8Value(const unsigned& value, v8::Handle<v8::Object>, v8::Isolate* isolate)
    {
        return v8::Integer::NewFromUnsigned(isolate, value);
    }
};

template<>
struct V8ValueTraits<unsigned long> {
    static inline v8::Handle<v8::Value> toV8Value(const unsigned long& value, v8::Handle<v8::Object>, v8::Isolate* isolate)
    {
        return v8::Integer::NewFromUnsigned(isolate, value);
    }
};

template<>
struct V8ValueTraits<float> {
    static inline v8::Handle<v8::Value> toV8Value(const float& value, v8::Handle<v8::Object>, v8::Isolate* isolate)
    {
        return v8::Number::New(isolate, value);
    }
};

template<>
struct V8ValueTraits<double> {
    static inline v8::Handle<v8::Value> toV8Value(const double& value, v8::Handle<v8::Object>, v8::Isolate* isolate)
    {
        return v8::Number::New(isolate, value);
    }
};

template<>
struct V8ValueTraits<bool> {
    static inline v8::Handle<v8::Value> toV8Value(const bool& value, v8::Handle<v8::Object>, v8::Isolate* isolate)
    {
        return v8::Boolean::New(isolate, value);
    }
};

// V8NullType and V8UndefinedType are used only for the value conversion.
class V8NullType { };
class V8UndefinedType { };

template<>
struct V8ValueTraits<V8NullType> {
    static inline v8::Handle<v8::Value> toV8Value(const V8NullType&, v8::Handle<v8::Object>, v8::Isolate* isolate)
    {
        return v8::Null(isolate);
    }
};

template<>
struct V8ValueTraits<V8UndefinedType> {
    static inline v8::Handle<v8::Value> toV8Value(const V8UndefinedType&, v8::Handle<v8::Object>, v8::Isolate* isolate)
    {
        return v8::Undefined(isolate);
    }
};

template<>
struct V8ValueTraits<ScriptValue> {
    static inline v8::Handle<v8::Value> toV8Value(const ScriptValue& value, v8::Handle<v8::Object>, v8::Isolate*)
    {
        return value.v8Value();
    }
};

template<>
struct V8ValueTraits<v8::Handle<v8::Value> > {
    static inline v8::Handle<v8::Value> toV8Value(const v8::Handle<v8::Value>& value, v8::Handle<v8::Object>, v8::Isolate*)
    {
        return value;
    }
};

template<>
struct V8ValueTraits<v8::Local<v8::Value> > {
    static inline v8::Handle<v8::Value> toV8Value(const v8::Local<v8::Value>& value, v8::Handle<v8::Object>, v8::Isolate*)
    {
        return value;
    }
};

template<typename T, size_t inlineCapacity>
v8::Handle<v8::Value> v8Array(const Vector<T, inlineCapacity>& iterator, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    v8::Local<v8::Array> result = v8::Array::New(isolate, iterator.size());
    int index = 0;
    typename Vector<T, inlineCapacity>::const_iterator end = iterator.end();
    typedef V8ValueTraits<T> TraitsType;
    for (typename Vector<T, inlineCapacity>::const_iterator iter = iterator.begin(); iter != end; ++iter)
        result->Set(v8::Integer::New(isolate, index++), TraitsType::toV8Value(*iter, creationContext, isolate));
    return result;
}

template <typename T, size_t inlineCapacity, typename Allocator>
struct V8ValueTraits<WTF::Vector<T, inlineCapacity, Allocator> > {
    static v8::Handle<v8::Value> toV8Value(const Vector<T, inlineCapacity, Allocator>& value, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
    {
        return v8Array(value, creationContext, isolate);
    }
};

template<typename T, size_t inlineCapacity>
v8::Handle<v8::Value> v8Array(const HeapVector<T, inlineCapacity>& iterator, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    v8::Local<v8::Array> result = v8::Array::New(isolate, iterator.size());
    int index = 0;
    typename HeapVector<T, inlineCapacity>::const_iterator end = iterator.end();
    typedef V8ValueTraits<T> TraitsType;
    for (typename HeapVector<T, inlineCapacity>::const_iterator iter = iterator.begin(); iter != end; ++iter)
        result->Set(v8::Integer::New(isolate, index++), TraitsType::toV8Value(*iter, creationContext, isolate));
    return result;
}

template <typename T, size_t inlineCapacity>
struct V8ValueTraits<HeapVector<T, inlineCapacity> > {
    static v8::Handle<v8::Value> toV8Value(const HeapVector<T, inlineCapacity>& value, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
    {
        return v8Array(value, creationContext, isolate);
    }
};

// Conversion flags, used in toIntXX/toUIntXX.
enum IntegerConversionConfiguration {
    NormalConversion,
    EnforceRange,
    Clamp
};

// Convert a value to a 8-bit signed integer. The conversion fails if the
// value cannot be converted to a number or the range violated per WebIDL:
// http://www.w3.org/TR/WebIDL/#es-byte
int8_t toInt8(v8::Handle<v8::Value>, IntegerConversionConfiguration, ExceptionState&);
inline int8_t toInt8(v8::Handle<v8::Value> value, ExceptionState& exceptionState)
{
    return toInt8(value, NormalConversion, exceptionState);
}

// Convert a value to a 8-bit integer assuming the conversion cannot fail.
int8_t toInt8(v8::Handle<v8::Value>);

// Convert a value to a 8-bit unsigned integer. The conversion fails if the
// value cannot be converted to a number or the range violated per WebIDL:
// http://www.w3.org/TR/WebIDL/#es-octet
uint8_t toUInt8(v8::Handle<v8::Value>, IntegerConversionConfiguration, ExceptionState&);
inline uint8_t toUInt8(v8::Handle<v8::Value> value, ExceptionState& exceptionState)
{
    return toUInt8(value, NormalConversion, exceptionState);
}

// Convert a value to a 8-bit unsigned integer assuming the conversion cannot fail.
uint8_t toUInt8(v8::Handle<v8::Value>);

// Convert a value to a 16-bit signed integer. The conversion fails if the
// value cannot be converted to a number or the range violated per WebIDL:
// http://www.w3.org/TR/WebIDL/#es-short
int16_t toInt16(v8::Handle<v8::Value>, IntegerConversionConfiguration, ExceptionState&);
inline int16_t toInt16(v8::Handle<v8::Value> value, ExceptionState& exceptionState)
{
    return toInt16(value, NormalConversion, exceptionState);
}

// Convert a value to a 16-bit integer assuming the conversion cannot fail.
int16_t toInt16(v8::Handle<v8::Value>);

// Convert a value to a 16-bit unsigned integer. The conversion fails if the
// value cannot be converted to a number or the range violated per WebIDL:
// http://www.w3.org/TR/WebIDL/#es-unsigned-short
uint16_t toUInt16(v8::Handle<v8::Value>, IntegerConversionConfiguration, ExceptionState&);
inline uint16_t toUInt16(v8::Handle<v8::Value> value, ExceptionState& exceptionState)
{
    return toUInt16(value, NormalConversion, exceptionState);
}

// Convert a value to a 16-bit unsigned integer assuming the conversion cannot fail.
uint16_t toUInt16(v8::Handle<v8::Value>);

// Convert a value to a 32-bit signed integer. The conversion fails if the
// value cannot be converted to a number or the range violated per WebIDL:
// http://www.w3.org/TR/WebIDL/#es-long
int32_t toInt32(v8::Handle<v8::Value>, IntegerConversionConfiguration, ExceptionState&);
inline int32_t toInt32(v8::Handle<v8::Value> value, ExceptionState& exceptionState)
{
    return toInt32(value, NormalConversion, exceptionState);
}

// Convert a value to a 32-bit integer assuming the conversion cannot fail.
int32_t toInt32(v8::Handle<v8::Value>);

// Convert a value to a 32-bit unsigned integer. The conversion fails if the
// value cannot be converted to a number or the range violated per WebIDL:
// http://www.w3.org/TR/WebIDL/#es-unsigned-long
uint32_t toUInt32(v8::Handle<v8::Value>, IntegerConversionConfiguration, ExceptionState&);
inline uint32_t toUInt32(v8::Handle<v8::Value> value, ExceptionState& exceptionState)
{
    return toUInt32(value, NormalConversion, exceptionState);
}

// Convert a value to a 32-bit unsigned integer assuming the conversion cannot fail.
uint32_t toUInt32(v8::Handle<v8::Value>);

// Convert a value to a 64-bit signed integer. The conversion fails if the
// value cannot be converted to a number or the range violated per WebIDL:
// http://www.w3.org/TR/WebIDL/#es-long-long
int64_t toInt64(v8::Handle<v8::Value>, IntegerConversionConfiguration, ExceptionState&);
inline int64_t toInt64(v8::Handle<v8::Value> value, ExceptionState& exceptionState)
{
    return toInt64(value, NormalConversion, exceptionState);
}

// Convert a value to a 64-bit integer assuming the conversion cannot fail.
int64_t toInt64(v8::Handle<v8::Value>);

// Convert a value to a 64-bit unsigned integer. The conversion fails if the
// value cannot be converted to a number or the range violated per WebIDL:
// http://www.w3.org/TR/WebIDL/#es-unsigned-long-long
uint64_t toUInt64(v8::Handle<v8::Value>, IntegerConversionConfiguration, ExceptionState&);
inline uint64_t toUInt64(v8::Handle<v8::Value> value, ExceptionState& exceptionState)
{
    return toUInt64(value, NormalConversion, exceptionState);
}

// Convert a value to a 64-bit unsigned integer assuming the conversion cannot fail.
uint64_t toUInt64(v8::Handle<v8::Value>);

// Convert a value to a single precision float, which might fail.
float toFloat(v8::Handle<v8::Value>, ExceptionState&);

// Convert a value to a single precision float assuming the conversion cannot fail.
inline float toFloat(v8::Local<v8::Value> value)
{
    return static_cast<float>(value->NumberValue());
}

// Convert a value to a double precision float, which might fail.
double toDouble(v8::Handle<v8::Value>, ExceptionState&);

// Converts a value to a String, throwing if any code unit is outside 0-255.
String toByteString(v8::Handle<v8::Value>, ExceptionState&);

// Converts a value to a String, replacing unmatched UTF-16 surrogates with replacement characters.
String toUSVString(v8::Handle<v8::Value>, ExceptionState&);

inline v8::Handle<v8::Boolean> v8Boolean(bool value, v8::Isolate* isolate)
{
    return value ? v8::True(isolate) : v8::False(isolate);
}

inline double toCoreDate(v8::Handle<v8::Value> object)
{
    if (object->IsDate())
        return v8::Handle<v8::Date>::Cast(object)->ValueOf();
    if (object->IsNumber())
        return object->NumberValue();
    return std::numeric_limits<double>::quiet_NaN();
}

inline v8::Handle<v8::Value> v8DateOrNaN(double value, v8::Isolate* isolate)
{
    ASSERT(isolate);
    return v8::Date::New(isolate, std::isfinite(value) ? value : std::numeric_limits<double>::quiet_NaN());
}

// FIXME: Remove the special casing for NodeFilter and XPathNSResolver.
PassRefPtrWillBeRawPtr<NodeFilter> toNodeFilter(v8::Handle<v8::Value>, v8::Handle<v8::Object>, ScriptState*);
PassRefPtrWillBeRawPtr<XPathNSResolver> toXPathNSResolver(v8::Isolate*, v8::Handle<v8::Value>);

template<class T> struct NativeValueTraits;

bool toV8Sequence(v8::Handle<v8::Value>, uint32_t& length, v8::Isolate*, ExceptionState&);

// Converts a JavaScript value to an array as per the Web IDL specification:
// http://www.w3.org/TR/2012/CR-WebIDL-20120419/#es-array
template <class T, class V8T>
Vector<RefPtr<T> > toRefPtrNativeArrayUnchecked(v8::Local<v8::Value> v8Value, uint32_t length, v8::Isolate* isolate, ExceptionState& exceptionState)
{
    Vector<RefPtr<T> > result;
    result.reserveInitialCapacity(length);
    v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(v8Value);
    v8::TryCatch block;
    for (uint32_t i = 0; i < length; ++i) {
        v8::Handle<v8::Value> element = object->Get(i);
        if (block.HasCaught()) {
            exceptionState.rethrowV8Exception(block.Exception());
            return Vector<RefPtr<T> >();
        }
        if (V8T::hasInstance(element, isolate)) {
            v8::Handle<v8::Object> elementObject = v8::Handle<v8::Object>::Cast(element);
            result.uncheckedAppend(V8T::toImpl(elementObject));
        } else {
            exceptionState.throwTypeError("Invalid Array element type");
            return Vector<RefPtr<T> >();
        }
    }
    return result;
}

template <class T, class V8T>
Vector<RefPtr<T> > toRefPtrNativeArray(v8::Handle<v8::Value> value, int argumentIndex, v8::Isolate* isolate, ExceptionState& exceptionState)
{
    v8::Local<v8::Value> v8Value(v8::Local<v8::Value>::New(isolate, value));
    uint32_t length = 0;
    if (value->IsArray()) {
        length = v8::Local<v8::Array>::Cast(v8Value)->Length();
    } else if (!toV8Sequence(value, length, isolate, exceptionState)) {
        if (!exceptionState.hadException())
            exceptionState.throwTypeError(ExceptionMessages::notAnArrayTypeArgumentOrValue(argumentIndex));
        return Vector<RefPtr<T> >();
    }
    return toRefPtrNativeArrayUnchecked<T, V8T>(v8Value, length, isolate, exceptionState);
}

template <class T, class V8T>
Vector<RefPtr<T> > toRefPtrNativeArray(v8::Handle<v8::Value> value, const String& propertyName, v8::Isolate* isolate, ExceptionState& exceptionState)
{
    v8::Local<v8::Value> v8Value(v8::Local<v8::Value>::New(isolate, value));
    uint32_t length = 0;
    if (value->IsArray()) {
        length = v8::Local<v8::Array>::Cast(v8Value)->Length();
    } else if (!toV8Sequence(value, length, isolate, exceptionState)) {
        if (!exceptionState.hadException())
            exceptionState.throwTypeError(ExceptionMessages::notASequenceTypeProperty(propertyName));
        return Vector<RefPtr<T> >();
    }
    return toRefPtrNativeArrayUnchecked<T, V8T>(v8Value, length, isolate, exceptionState);
}

template <class T, class V8T>
WillBeHeapVector<RefPtrWillBeMember<T> > toRefPtrWillBeMemberNativeArray(v8::Handle<v8::Value> value, int argumentIndex, v8::Isolate* isolate, ExceptionState& exceptionState)
{
    v8::Local<v8::Value> v8Value(v8::Local<v8::Value>::New(isolate, value));
    uint32_t length = 0;
    if (value->IsArray()) {
        length = v8::Local<v8::Array>::Cast(v8Value)->Length();
    } else if (!toV8Sequence(value, length, isolate, exceptionState)) {
        if (!exceptionState.hadException())
            exceptionState.throwTypeError(ExceptionMessages::notAnArrayTypeArgumentOrValue(argumentIndex));
        return WillBeHeapVector<RefPtrWillBeMember<T> >();
    }

    WillBeHeapVector<RefPtrWillBeMember<T> > result;
    result.reserveInitialCapacity(length);
    v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(v8Value);
    v8::TryCatch block;
    for (uint32_t i = 0; i < length; ++i) {
        v8::Handle<v8::Value> element = object->Get(i);
        if (block.HasCaught()) {
            exceptionState.rethrowV8Exception(block.Exception());
            return WillBeHeapVector<RefPtrWillBeMember<T> >();
        }
        if (V8T::hasInstance(element, isolate)) {
            v8::Handle<v8::Object> elementObject = v8::Handle<v8::Object>::Cast(element);
            result.uncheckedAppend(V8T::toImpl(elementObject));
        } else {
            exceptionState.throwTypeError("Invalid Array element type");
            return WillBeHeapVector<RefPtrWillBeMember<T> >();
        }
    }
    return result;
}

template <class T, class V8T>
WillBeHeapVector<RefPtrWillBeMember<T> > toRefPtrWillBeMemberNativeArray(v8::Handle<v8::Value> value, const String& propertyName, v8::Isolate* isolate, ExceptionState& exceptionState)
{
    v8::Local<v8::Value> v8Value(v8::Local<v8::Value>::New(isolate, value));
    uint32_t length = 0;
    if (value->IsArray()) {
        length = v8::Local<v8::Array>::Cast(v8Value)->Length();
    } else if (!toV8Sequence(value, length, isolate, exceptionState)) {
        if (!exceptionState.hadException())
            exceptionState.throwTypeError(ExceptionMessages::notASequenceTypeProperty(propertyName));
        return WillBeHeapVector<RefPtrWillBeMember<T> >();
    }

    WillBeHeapVector<RefPtrWillBeMember<T> > result;
    result.reserveInitialCapacity(length);
    v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(v8Value);
    v8::TryCatch block;
    for (uint32_t i = 0; i < length; ++i) {
        v8::Handle<v8::Value> element = object->Get(i);
        if (block.HasCaught()) {
            exceptionState.rethrowV8Exception(block.Exception());
            return WillBeHeapVector<RefPtrWillBeMember<T> >();
        }
        if (V8T::hasInstance(element, isolate)) {
            v8::Handle<v8::Object> elementObject = v8::Handle<v8::Object>::Cast(element);
            result.uncheckedAppend(V8T::toImpl(elementObject));
        } else {
            exceptionState.throwTypeError("Invalid Array element type");
            return WillBeHeapVector<RefPtrWillBeMember<T> >();
        }
    }
    return result;
}

template <class T, class V8T>
HeapVector<Member<T> > toMemberNativeArray(v8::Handle<v8::Value> value, int argumentIndex, v8::Isolate* isolate, ExceptionState& exceptionState)
{
    v8::Local<v8::Value> v8Value(v8::Local<v8::Value>::New(isolate, value));
    uint32_t length = 0;
    if (value->IsArray()) {
        length = v8::Local<v8::Array>::Cast(v8Value)->Length();
    } else if (!toV8Sequence(value, length, isolate, exceptionState)) {
        if (!exceptionState.hadException())
            exceptionState.throwTypeError(ExceptionMessages::notAnArrayTypeArgumentOrValue(argumentIndex));
        return HeapVector<Member<T> >();
    }

    HeapVector<Member<T> > result;
    result.reserveInitialCapacity(length);
    v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(v8Value);
    v8::TryCatch block;
    for (uint32_t i = 0; i < length; ++i) {
        v8::Handle<v8::Value> element = object->Get(i);
        if (UNLIKELY(block.HasCaught())) {
            exceptionState.rethrowV8Exception(block.Exception());
            return HeapVector<Member<T> >();
        }
        if (V8T::hasInstance(element, isolate)) {
            v8::Handle<v8::Object> elementObject = v8::Handle<v8::Object>::Cast(element);
            result.uncheckedAppend(V8T::toImpl(elementObject));
        } else {
            exceptionState.throwTypeError("Invalid Array element type");
            return HeapVector<Member<T> >();
        }
    }
    return result;
}

// Converts a JavaScript value to an array as per the Web IDL specification:
// http://www.w3.org/TR/2012/CR-WebIDL-20120419/#es-array
template <class T>
Vector<T> toImplArray(v8::Handle<v8::Value> value, int argumentIndex, v8::Isolate* isolate, ExceptionState& exceptionState)
{
    v8::Local<v8::Value> v8Value(v8::Local<v8::Value>::New(isolate, value));
    uint32_t length = 0;
    if (value->IsArray()) {
        length = v8::Local<v8::Array>::Cast(v8Value)->Length();
    } else if (!toV8Sequence(value, length, isolate, exceptionState)) {
        if (!exceptionState.hadException())
            exceptionState.throwTypeError(ExceptionMessages::notAnArrayTypeArgumentOrValue(argumentIndex));
        return Vector<T>();
    }

    Vector<T> result;
    result.reserveInitialCapacity(length);
    typedef NativeValueTraits<T> TraitsType;
    v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(v8Value);
    v8::TryCatch block;
    for (uint32_t i = 0; i < length; ++i) {
        v8::Handle<v8::Value> element = object->Get(i);
        if (UNLIKELY(block.HasCaught())) {
            exceptionState.rethrowV8Exception(block.Exception());
            return Vector<T>();
        }
        result.uncheckedAppend(TraitsType::nativeValue(element, isolate, exceptionState));
        if (exceptionState.hadException())
            return Vector<T>();
    }
    return result;
}

template <class T>
Vector<T> toImplArguments(const v8::FunctionCallbackInfo<v8::Value>& info, int startIndex, ExceptionState& exceptionState)
{
    ASSERT(startIndex <= info.Length());
    Vector<T> result;
    typedef NativeValueTraits<T> TraitsType;
    int length = info.Length();
    result.reserveInitialCapacity(length);
    for (int i = startIndex; i < length; ++i) {
        result.uncheckedAppend(TraitsType::nativeValue(info[i], info.GetIsolate(), exceptionState));
        if (exceptionState.hadException())
            return Vector<T>();
    }
    return result;
}

// Validates that the passed object is a sequence type per WebIDL spec
// http://www.w3.org/TR/2012/CR-WebIDL-20120419/#es-sequence
inline bool toV8Sequence(v8::Handle<v8::Value> value, uint32_t& length, v8::Isolate* isolate, ExceptionState& exceptionState)
{
    // Attempt converting to a sequence if the value is not already an array but is
    // any kind of object except for a native Date object or a native RegExp object.
    ASSERT(!value->IsArray());
    // FIXME: Do we really need to special case Date and RegExp object?
    // https://www.w3.org/Bugs/Public/show_bug.cgi?id=22806
    if (!value->IsObject() || value->IsDate() || value->IsRegExp()) {
        // The caller is responsible for reporting a TypeError.
        return false;
    }

    v8::Local<v8::Value> v8Value(v8::Local<v8::Value>::New(isolate, value));
    v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(v8Value);
    v8::Local<v8::String> lengthSymbol = v8AtomicString(isolate, "length");

    // FIXME: The specification states that the length property should be used as fallback, if value
    // is not a platform object that supports indexed properties. If it supports indexed properties,
    // length should actually be one greater than value’s maximum indexed property index.
    v8::TryCatch block;
    v8::Local<v8::Value> lengthValue = object->Get(lengthSymbol);
    if (block.HasCaught()) {
        exceptionState.rethrowV8Exception(block.Exception());
        return false;
    }

    if (lengthValue->IsUndefined() || lengthValue->IsNull()) {
        // The caller is responsible for reporting a TypeError.
        return false;
    }

    uint32_t sequenceLength = lengthValue->Int32Value();
    if (block.HasCaught()) {
        exceptionState.rethrowV8Exception(block.Exception());
        return false;
    }

    length = sequenceLength;
    return true;
}

template<>
struct NativeValueTraits<String> {
    static inline String nativeValue(const v8::Handle<v8::Value>& value, v8::Isolate* isolate, ExceptionState& exceptionState)
    {
        V8StringResource<> stringValue(value);
        if (!stringValue.prepare(exceptionState))
            return String();
        return stringValue;
    }
};

template<>
struct NativeValueTraits<int> {
    static inline int nativeValue(const v8::Handle<v8::Value>& value, v8::Isolate* isolate, ExceptionState& exceptionState)
    {
        return toInt32(value, exceptionState);
    }
};

template<>
struct NativeValueTraits<unsigned> {
    static inline unsigned nativeValue(const v8::Handle<v8::Value>& value, v8::Isolate* isolate, ExceptionState& exceptionState)
    {
        return toUInt32(value, exceptionState);
    }
};

template<>
struct NativeValueTraits<float> {
    static inline float nativeValue(const v8::Handle<v8::Value>& value, v8::Isolate* isolate, ExceptionState& exceptionState)
    {
        return toFloat(value, exceptionState);
    }
};

template<>
struct NativeValueTraits<double> {
    static inline double nativeValue(const v8::Handle<v8::Value>& value, v8::Isolate* isolate, ExceptionState& exceptionState)
    {
        return toDouble(value, exceptionState);
    }
};

template<>
struct NativeValueTraits<v8::Handle<v8::Value> > {
    static inline v8::Handle<v8::Value> nativeValue(const v8::Handle<v8::Value>& value, v8::Isolate* isolate, ExceptionState&)
    {
        return value;
    }
};

template<>
struct NativeValueTraits<ScriptValue> {
    static inline ScriptValue nativeValue(const v8::Handle<v8::Value>& value, v8::Isolate* isolate, ExceptionState&)
    {
        return ScriptValue(ScriptState::current(isolate), value);
    }
};

template <typename T>
struct NativeValueTraits<Vector<T> > {
    static inline Vector<T> nativeValue(const v8::Handle<v8::Value>& value, v8::Isolate* isolate, ExceptionState& exceptionState)
    {
        return toImplArray<T>(value, 0, isolate, exceptionState);
    }
};

v8::Isolate* toIsolate(ExecutionContext*);
v8::Isolate* toIsolate(LocalFrame*);

LocalDOMWindow* toDOMWindow(v8::Handle<v8::Value>, v8::Isolate*);
LocalDOMWindow* toDOMWindow(v8::Handle<v8::Context>);
LocalDOMWindow* enteredDOMWindow(v8::Isolate*);
LocalDOMWindow* currentDOMWindow(v8::Isolate*);
LocalDOMWindow* callingDOMWindow(v8::Isolate*);
ExecutionContext* toExecutionContext(v8::Handle<v8::Context>);
ExecutionContext* currentExecutionContext(v8::Isolate*);
ExecutionContext* callingExecutionContext(v8::Isolate*);

// Returns a V8 context associated with a ExecutionContext and a DOMWrapperWorld.
// This method returns an empty context if there is no frame or the frame is already detached.
v8::Local<v8::Context> toV8Context(ExecutionContext*, DOMWrapperWorld&);
// Returns a V8 context associated with a LocalFrame and a DOMWrapperWorld.
// This method returns an empty context if the frame is already detached.
v8::Local<v8::Context> toV8Context(LocalFrame*, DOMWrapperWorld&);

// Returns the frame object of the window object associated with
// a context, if the window is currently being displayed in the LocalFrame.
LocalFrame* toFrameIfNotDetached(v8::Handle<v8::Context>);

// If the current context causes out of memory, JavaScript setting
// is disabled and it returns true.
bool handleOutOfMemory();
void crashIfV8IsDead();

inline bool isUndefinedOrNull(v8::Handle<v8::Value> value)
{
    return value->IsNull() || value->IsUndefined();
}
v8::Handle<v8::Function> getBoundFunction(v8::Handle<v8::Function>);

// Attaches |environment| to |function| and returns it.
inline v8::Local<v8::Function> createClosure(v8::FunctionCallback function, v8::Handle<v8::Value> environment, v8::Isolate* isolate)
{
    return v8::Function::New(isolate, function, environment);
}

// FIXME: This will be soon embedded in the generated code.
template<class Collection> static void indexedPropertyEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info)
{
    Collection* collection = toScriptWrappableBase(info.Holder())->toImpl<Collection>();
    int length = collection->length();
    v8::Handle<v8::Array> properties = v8::Array::New(info.GetIsolate(), length);
    for (int i = 0; i < length; ++i) {
        // FIXME: Do we need to check that the item function returns a non-null value for this index?
        v8::Handle<v8::Integer> integer = v8::Integer::New(info.GetIsolate(), i);
        properties->Set(integer, integer);
    }
    v8SetReturnValue(info, properties);
}

// These methods store hidden values into an array that is stored in the internal field of a DOM wrapper.
void addHiddenValueToArray(v8::Isolate*, v8::Handle<v8::Object>, v8::Local<v8::Value>, int cacheIndex);
void removeHiddenValueFromArray(v8::Isolate*, v8::Handle<v8::Object>, v8::Local<v8::Value>, int cacheIndex);
void moveEventListenerToNewWrapper(v8::Isolate*, v8::Handle<v8::Object>, EventListener* oldValue, v8::Local<v8::Value> newValue, int cacheIndex);

PassRefPtr<JSONValue> v8ToJSONValue(v8::Isolate*, v8::Handle<v8::Value>, int);

// Result values for platform object 'deleter' methods,
// http://www.w3.org/TR/WebIDL/#delete
enum DeleteResult {
    DeleteSuccess,
    DeleteReject,
    DeleteUnknownProperty
};

class V8IsolateInterruptor : public ThreadState::Interruptor {
public:
    explicit V8IsolateInterruptor(v8::Isolate* isolate) : m_isolate(isolate) { }

    static void onInterruptCallback(v8::Isolate* isolate, void* data)
    {
        reinterpret_cast<V8IsolateInterruptor*>(data)->onInterrupted();
    }

    virtual void requestInterrupt() override
    {
        m_isolate->RequestInterrupt(&onInterruptCallback, this);
    }

    virtual void clearInterrupt() override
    {
        m_isolate->ClearInterrupt();
    }

private:
    v8::Isolate* m_isolate;
};

class V8TestingScope {
public:
    explicit V8TestingScope(v8::Isolate*);
    ScriptState* scriptState() const;
    v8::Isolate* isolate() const;
    ~V8TestingScope();

private:
    v8::HandleScope m_handleScope;
    v8::Context::Scope m_contextScope;
    RefPtr<ScriptState> m_scriptState;
};

void GetDevToolsFunctionInfo(v8::Handle<v8::Function>, v8::Isolate*, int& scriptId, String& resourceName, int& lineNumber);
PassRefPtr<TraceEvent::ConvertableToTraceFormat> devToolsTraceEventData(v8::Isolate*, ExecutionContext*, v8::Handle<v8::Function>);

class V8RethrowTryCatchScope final {
public:
    explicit V8RethrowTryCatchScope(v8::TryCatch& block) : m_block(block) { }
    ~V8RethrowTryCatchScope()
    {
        // ReThrow() is a no-op if no exception has been caught, so always call.
        m_block.ReThrow();
    }

private:
    v8::TryCatch& m_block;
};

// Returns an object representing {done: true, value: undefined}.
v8::Local<v8::Value> v8DoneIteratorResult(v8::Isolate*);

// Returns an object representing {done: false, value: |value|}.
v8::Local<v8::Value> v8IteratorResult(v8::Isolate*, v8::Handle<v8::Value>);
template <typename T>
v8::Local<v8::Value> v8IteratorResult(ScriptState* scriptState, const T& value)
{
    return v8IteratorResult(scriptState->isolate(), V8ValueTraits<T>::toV8Value(value, scriptState->context()->Global(), scriptState->isolate()));
}

typedef void (*InstallTemplateFunction)(v8::Handle<v8::FunctionTemplate>, v8::Isolate*);

} // namespace blink

#endif // V8Binding_h
