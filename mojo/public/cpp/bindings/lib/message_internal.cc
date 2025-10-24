// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/message_internal.h"

#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {
namespace internal {

size_t ComputeSerializedMessageSize(size_t payload_size,
                                    size_t payload_interface_id_count) {
  const size_t header_size = sizeof(MessageHeaderV3);
  if (payload_interface_id_count > 0) {
    return Align(header_size + Align(payload_size) +
                 ArrayDataTraits<uint32_t>::GetStorageSize(
                     static_cast<uint32_t>(payload_interface_id_count)));
  }
  return internal::Align(header_size + payload_size);
}

size_t EstimateSerializedMessageSize(uint32_t message_name,
                                     size_t payload_size,
                                     size_t total_size,
                                     size_t estimated_payload_size) {
  if (estimated_payload_size > payload_size) {
    const size_t extra_payload_size = estimated_payload_size - payload_size;
    return internal::Align(total_size + extra_payload_size);
  }
  return total_size;
}

}  // namespace internal
}  // namespace mojo
