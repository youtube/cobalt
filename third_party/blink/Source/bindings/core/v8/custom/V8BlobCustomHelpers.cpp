/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "config.h"
#include "bindings/core/v8/custom/V8BlobCustomHelpers.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8ArrayBuffer.h"
#include "bindings/core/v8/V8ArrayBufferView.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8Blob.h"
#include "core/dom/ExceptionCode.h"
#include "wtf/DateMath.h"

namespace blink {

namespace V8BlobCustomHelpers {

ParsedProperties::ParsedProperties(bool hasFileProperties)
    : m_normalizeLineEndingsToNative(false)
    , m_hasFileProperties(hasFileProperties)
#if ENABLE(ASSERT)
    , m_hasLastModified(false)
#endif // ENABLE(ASSERT)
{
}

void ParsedProperties::setLastModified(double lastModified)
{
    ASSERT(m_hasFileProperties);
    ASSERT(!m_hasLastModified);
    m_lastModified = lastModified;
#if ENABLE(ASSERT)
    m_hasLastModified = true;
#endif // ENABLE(ASSERT)
}

void ParsedProperties::setDefaultLastModified()
{
    setLastModified(currentTime());
}

bool ParsedProperties::parseBlobPropertyBag(v8::Isolate* isolate, v8::Local<v8::Value> propertyBag, const char* blobClassName, ExceptionState& exceptionState)
{
    TONATIVE_DEFAULT(Dictionary, dictionary, Dictionary(propertyBag, isolate), false);

    String endings;
    TONATIVE_DEFAULT(bool, containsEndings, DictionaryHelper::get(dictionary, "endings", endings), false);
    if (containsEndings) {
        if (endings != "transparent" && endings != "native") {
            exceptionState.throwTypeError("The 'endings' property must be either 'transparent' or 'native'.");
            return false;
        }
        if (endings == "native")
            m_normalizeLineEndingsToNative = true;
    }

    TONATIVE_DEFAULT(bool, containsType, DictionaryHelper::get(dictionary, "type", m_contentType), false);
    if (containsType) {
        if (!m_contentType.containsOnlyASCII()) {
            exceptionState.throwDOMException(SyntaxError, "The 'type' property must consist of ASCII characters.");
            return false;
        }
        m_contentType = m_contentType.lower();
    }

    if (!m_hasFileProperties)
        return true;

    v8::Local<v8::Value> lastModified;
    TONATIVE_DEFAULT(bool, containsLastModified, DictionaryHelper::get(dictionary, "lastModified", lastModified), false);
    if (containsLastModified) {
        TONATIVE_DEFAULT(long long, lastModifiedInt, toInt64(lastModified), false);
        setLastModified(static_cast<double>(lastModifiedInt) / msPerSecond);
    } else {
        setDefaultLastModified();
    }

    return true;
}

bool processBlobParts(v8::Isolate* isolate, v8::Local<v8::Object> blobParts, bool normalizeLineEndingsToNative, BlobData& blobData)
{
    // FIXME: handle sequences based on ES6 @@iterator, see http://crbug.com/393866
    v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(blobParts);
    uint32_t length = v8::Local<v8::Array>::Cast(blobParts)->Length();
    for (uint32_t i = 0; i < length; ++i) {
        v8::Local<v8::Value> item = array->Get(i);
        if (item.IsEmpty())
            return false;

        if (V8ArrayBuffer::hasInstance(item, isolate)) {
            DOMArrayBuffer* arrayBuffer = V8ArrayBuffer::toImpl(v8::Handle<v8::Object>::Cast(item));
            ASSERT(arrayBuffer);
            blobData.appendArrayBuffer(arrayBuffer->buffer());
        } else if (V8ArrayBufferView::hasInstance(item, isolate)) {
            DOMArrayBufferView* arrayBufferView = V8ArrayBufferView::toImpl(v8::Handle<v8::Object>::Cast(item));
            ASSERT(arrayBufferView);
            blobData.appendArrayBufferView(arrayBufferView->view());
        } else if (V8Blob::hasInstance(item, isolate)) {
            Blob* blob = V8Blob::toImpl(v8::Handle<v8::Object>::Cast(item));
            ASSERT(blob);
            blob->appendTo(blobData);
        } else {
            TOSTRING_DEFAULT(V8StringResource<>, stringValue, item, false);
            blobData.appendText(stringValue, normalizeLineEndingsToNative);
        }
    }
    return true;
}

} // namespace V8BlobCustomHelpers

} // namespace blink
