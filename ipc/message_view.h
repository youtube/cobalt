// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_MESSAGE_VIEW_H_
#define IPC_MESSAGE_VIEW_H_

#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "ipc/ipc_message.h"
#include "mojo/public/interfaces/bindings/native_struct.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace IPC {

class COMPONENT_EXPORT(IPC_MOJOM) MessageView {
 public:
  MessageView();
  MessageView(
      base::span<const uint8_t> bytes,
      absl::optional<std::vector<mojo::native::SerializedHandlePtr>> handles);
  MessageView(MessageView&&);

  MessageView(const MessageView&) = delete;
  MessageView& operator=(const MessageView&) = delete;

  ~MessageView();

  MessageView& operator=(MessageView&&);

  base::span<const uint8_t> bytes() const { return bytes_; }
  absl::optional<std::vector<mojo::native::SerializedHandlePtr>> TakeHandles();

 private:
  base::span<const uint8_t> bytes_;
  absl::optional<std::vector<mojo::native::SerializedHandlePtr>> handles_;
};

}  // namespace IPC

#endif  // IPC_MESSAGE_VIEW_H_
