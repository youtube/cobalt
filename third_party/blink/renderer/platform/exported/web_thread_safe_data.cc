/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/public/platform/web_thread_safe_data.h"

#include "base/compiler_specific.h"
#include "base/containers/checked_iterators.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"

namespace blink {

WebThreadSafeData::WebThreadSafeData(base::span<const char> data) {
  private_ = RawData::Create();
  private_->MutableData()->AppendSpan(data);
}

void WebThreadSafeData::Reset() {
  private_.Reset();
}

void WebThreadSafeData::Assign(const WebThreadSafeData& other) {
  private_ = other.private_;
}

size_t WebThreadSafeData::size() const {
  return private_.IsNull() ? 0 : private_->size();
}

const char* WebThreadSafeData::data() const {
  return private_.IsNull() ? nullptr : private_->data();
}

WebThreadSafeData::iterator WebThreadSafeData::begin() const {
  // SAFETY: `data()` never points to fewer than `size()` bytes, so this is
  // never further than one-past-the-end.
  return UNSAFE_BUFFERS(iterator(data(), data() + size()));
}

WebThreadSafeData::iterator WebThreadSafeData::end() const {
  // SAFETY: As in `begin()` above.
  return UNSAFE_BUFFERS(iterator(data(), data() + size(), data() + size()));
}

WebThreadSafeData::WebThreadSafeData(scoped_refptr<RawData> data)
    : private_(std::move(data)) {}

WebThreadSafeData::WebThreadSafeData(scoped_refptr<RawData>&& data)
    : private_(std::move(data)) {}

WebThreadSafeData::WebThreadSafeData(const WebThreadSafeData& other) {
  private_ = other.private_;
}

WebThreadSafeData& WebThreadSafeData::operator=(
    const WebThreadSafeData& other) = default;

WebThreadSafeData& WebThreadSafeData::operator=(scoped_refptr<RawData> data) {
  private_ = std::move(data);
  return *this;
}

}  // namespace blink
