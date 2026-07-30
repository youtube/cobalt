// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/internal/immutable_string.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <numeric>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include "base/check.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"

namespace base::i18n::internal {

ImmutableString::HeapString::HeapString(const HeapString& other)
    : storage_(base::HeapArray<char>::CopiedFrom(other.storage_.as_span())) {}
ImmutableString::HeapString& ImmutableString::HeapString::operator=(
    const HeapString& other) {
  storage_ = base::HeapArray<char>::CopiedFrom(other.storage_.as_span());
  return *this;
}
ImmutableString::HeapString::HeapString(HeapString&&) = default;
ImmutableString::HeapString& ImmutableString::HeapString::operator=(
    HeapString&&) = default;

ImmutableString::HeapString::HeapString(
    base::span<const std::string_view> parts)
    : storage_(base::HeapArray<char>::Uninit(TotalSize(parts))) {
  CopyParts(parts, storage_.as_span());
}
ImmutableString::HeapString::~HeapString() = default;

std::string_view ImmutableString::HeapString::AsString() const {
  return std::string_view(storage_.data(), storage_.size());
}

std::string_view ImmutableString::StackString::AsString() const {
  return std::string_view(storage_.data(), size_);
}

ImmutableString::ImmutableString(base::span<const std::string_view> parts)
    : storage_((TotalSize(parts) <= ImmutableString::kSmallBufferSize)
                   ? StorageVariantType(StackString(parts))
                   : StorageVariantType(HeapString(parts))) {}

ImmutableString::ImmutableString(const ImmutableString& other) = default;
ImmutableString& ImmutableString::operator=(const ImmutableString& other) =
    default;
ImmutableString::ImmutableString(ImmutableString&&) = default;
ImmutableString& ImmutableString::operator=(ImmutableString&&) = default;

std::string_view ImmutableString::AsString() const {
  return std::visit([](const auto& storage) { return storage.AsString(); },
                    storage_);
}

}  // namespace base::i18n::internal
