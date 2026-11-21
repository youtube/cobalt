// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_PUBLIC_CPP_SYSTEM_MESSAGE_H_
#define CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_PUBLIC_CPP_SYSTEM_MESSAGE_H_

#include <string_view>
#include <vector>

#include "base/check.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/c/system/message_pipe.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/cpp/system/handle.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/cpp/system/system_export.h"

namespace mojo_legacy {

const MojoMessageHandle kInvalidMessageHandleValue =
    MOJO_LEGACY_MESSAGE_HANDLE_INVALID;

// Handle wrapper base class for a |MojoMessageHandle|.
class MessageHandle {
 public:
  MessageHandle() : value_(kInvalidMessageHandleValue) {}
  explicit MessageHandle(MojoMessageHandle value) : value_(value) {}
  ~MessageHandle() {}

  void swap(MessageHandle& other) {
    MojoMessageHandle temp = value_;
    value_ = other.value_;
    other.value_ = temp;
  }

  bool is_valid() const { return value_ != kInvalidMessageHandleValue; }

  const MojoMessageHandle& value() const { return value_; }
  MojoMessageHandle* mutable_value() { return &value_; }
  void set_value(MojoMessageHandle value) { value_ = value; }

  void Close() {
    DCHECK(is_valid());
    [[maybe_unused]] MojoResult result = MojoDestroyMessage(value_);
    DCHECK_EQ(MOJO_LEGACY_RESULT_OK, result);
  }

 private:
  MojoMessageHandle value_;
};

using ScopedMessageHandle = ScopedHandleBase<MessageHandle>;

inline MojoResult CreateMessage(ScopedMessageHandle* handle,
                                MojoCreateMessageFlags flags) {
  MojoCreateMessageOptions options = {};
  options.struct_size = sizeof(options);
  options.flags = flags;
  MojoMessageHandle raw_handle;
  MojoResult rv = MojoCreateMessage(&options, &raw_handle);
  if (rv != MOJO_LEGACY_RESULT_OK) {
    return rv;
  }

  handle->reset(MessageHandle(raw_handle));
  return MOJO_LEGACY_RESULT_OK;
}

inline MojoResult GetMessageData(MessageHandle message,
                                 void** buffer,
                                 uint32_t* num_bytes,
                                 std::vector<ScopedHandle>* handles,
                                 MojoGetMessageDataFlags flags) {
  DCHECK(message.is_valid());
  DCHECK(num_bytes);
  DCHECK(buffer);
  uint32_t num_handles = 0;

  MojoGetMessageDataOptions options;
  options.struct_size = sizeof(options);
  options.flags = flags;
  MojoResult rv = MojoGetMessageData(message.value(), &options, buffer,
                                     num_bytes, nullptr, &num_handles);
  if (rv != MOJO_LEGACY_RESULT_RESOURCE_EXHAUSTED) {
    if (handles) {
      handles->clear();
    }
    return rv;
  }

  handles->resize(num_handles);
  return MojoGetMessageData(message.value(), &options, buffer, num_bytes,
                            reinterpret_cast<MojoHandle*>(handles->data()),
                            &num_handles);
}

MOJO_LEGACY_CPP_SYSTEM_EXPORT MojoResult
NotifyBadMessage(MessageHandle message, const std::string_view& error);

}  // namespace mojo_legacy

#endif  // CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_PUBLIC_CPP_SYSTEM_MESSAGE_H_
