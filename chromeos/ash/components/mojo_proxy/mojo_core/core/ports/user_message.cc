// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/mojo_proxy/mojo_core/core/ports/user_message.h"

namespace mojo_legacy {
namespace core {
namespace ports {

UserMessage::UserMessage(const TypeInfo* type_info) : type_info_(type_info) {}

UserMessage::~UserMessage() = default;

bool UserMessage::WillBeRoutedExternally() {
  return true;
}

size_t UserMessage::GetSizeIfSerialized() const {
  return 0;
}

}  // namespace ports
}  // namespace core
}  // namespace mojo_legacy
