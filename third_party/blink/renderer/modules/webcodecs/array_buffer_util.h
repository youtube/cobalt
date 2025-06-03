// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_ARRAY_BUFFER_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_ARRAY_BUFFER_UTIL_H_

#include "base/containers/span.h"
#include "media/base/decoder_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybufferallowshared_arraybufferviewallowshared.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

using AllowSharedBufferSource =
    V8UnionArrayBufferAllowSharedOrArrayBufferViewAllowShared;

// Helper function for turning various DOMArray-like things into a pointer+size.
template <typename T>
base::span<T> AsSpan(const AllowSharedBufferSource* buffer_union) {
  switch (buffer_union->GetContentType()) {
    case AllowSharedBufferSource::ContentType::kArrayBufferAllowShared: {
      auto* buffer = buffer_union->GetAsArrayBufferAllowShared();
      return (buffer && !buffer->IsDetached())
                 ? base::span<T>(
                       reinterpret_cast<T*>(buffer->DataMaybeShared()),
                       buffer->ByteLength())
                 : base::span<T>();
    }
    case AllowSharedBufferSource::ContentType::kArrayBufferViewAllowShared: {
      auto* buffer = buffer_union->GetAsArrayBufferViewAllowShared().Get();
      return (buffer && !buffer->IsDetached())
                 ? base::span<T>(
                       reinterpret_cast<T*>(buffer->BaseAddressMaybeShared()),
                       buffer->byteLength())
                 : base::span<T>();
    }
  }
}

// Ensures that the underlying memory for `buffer_union` remains valid
// (owned by a returned instance of ArrayBufferContents)
// even if the buffer is detached or truncated by client activity.
ArrayBufferContents PinArrayBufferContent(
    const AllowSharedBufferSource* buffer_union);

// 1. Check if any on the array buffers from the `transfer_list` contain
//    `data_range` and return contents of that array buffer.
// 2. Detach all array buffers from in the `transfer_list`
ArrayBufferContents TransferArrayBufferForSpan(
    const HeapVector<Member<DOMArrayBuffer>>& transfer_list,
    base::span<const uint8_t> data_range,
    ExceptionState& exception_state,
    v8::Isolate* isolate);

// A wrapper around `ArrayBuffer` contents that allows to transfer ownership
// to `DecoderBuffer`.
class ArrayBufferContentsExternalMemory
    : public media::DecoderBuffer::ExternalMemory {
 public:
  explicit ArrayBufferContentsExternalMemory(ArrayBufferContents contents,
                                             base::span<const uint8_t> span)
      : media::DecoderBuffer::ExternalMemory(span),
        contents_(std::move(contents)) {
    // Check that `span` refers to the memory inside `contents`.
    auto* contents_data = static_cast<const uint8_t*>(contents_.Data());
    CHECK_GE(span.data(), contents_data);
    CHECK_LE(span.data() + span.size(), contents_data + contents_.DataLength());
  }

 private:
  ArrayBufferContents contents_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_ARRAY_BUFFER_UTIL_H_
