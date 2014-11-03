// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/modules/v8/V8CryptoKey.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8Uint8Array.h"
#include "public/platform/WebCryptoKeyAlgorithm.h"
#include "wtf/Uint8Array.h"

namespace blink {

class DictionaryBuilder : public blink::WebCryptoKeyAlgorithmDictionary {
public:
    DictionaryBuilder(v8::Local<v8::Object> holder, v8::Isolate* isolate)
        : m_holder(holder)
        , m_isolate(isolate)
        , m_dictionary(Dictionary::createEmpty(isolate))
    {
    }

    virtual void setString(const char* propertyName, const char* value)
    {
        m_dictionary.set(propertyName, value);
    }

    virtual void setUint(const char* propertyName, unsigned value)
    {
        m_dictionary.set(propertyName, value);
    }

    virtual void setAlgorithm(const char* propertyName, const blink::WebCryptoAlgorithm& algorithm)
    {
        ASSERT(algorithm.paramsType() == blink::WebCryptoAlgorithmParamsTypeNone);

        Dictionary algorithmValue = Dictionary::createEmpty(m_isolate);
        algorithmValue.set("name", blink::WebCryptoAlgorithm::lookupAlgorithmInfo(algorithm.id())->name);
        m_dictionary.set(propertyName, algorithmValue);
    }

    virtual void setUint8Array(const char* propertyName, const blink::WebVector<unsigned char>& vector)
    {
        m_dictionary.set(propertyName, toV8(DOMUint8Array::create(vector.data(), vector.size()), m_holder, m_isolate));
    }

    const Dictionary& dictionary() const { return m_dictionary; }

private:
    v8::Local<v8::Object> m_holder;
    v8::Isolate* m_isolate;
    Dictionary m_dictionary;
};

void V8CryptoKey::algorithmAttributeGetterCustom(const v8::PropertyCallbackInfo<v8::Value>& info)
{
    CryptoKey* impl = V8CryptoKey::toImpl(info.Holder());

    DictionaryBuilder builder(info.Holder(), info.GetIsolate());
    impl->key().algorithm().writeToDictionary(&builder);

    v8SetReturnValue(info, builder.dictionary().v8Value());
}

} // namespace blink
