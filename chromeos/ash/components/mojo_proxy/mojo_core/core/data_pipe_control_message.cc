// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/mojo_proxy/mojo_core/core/data_pipe_control_message.h"

#include "base/logging.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/core/node_controller.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/core/ports/event.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/core/user_message_impl.h"

namespace mojo_legacy {
namespace core {

void SendDataPipeControlMessage(NodeController* node_controller,
                                const ports::PortRef& port,
                                DataPipeCommand command,
                                uint32_t num_bytes) {
  std::unique_ptr<ports::UserMessageEvent> event;
  MojoResult result = UserMessageImpl::CreateEventForNewSerializedMessage(
      sizeof(DataPipeControlMessage), nullptr, 0, &event);
  DCHECK_EQ(MOJO_LEGACY_RESULT_OK, result);
  DCHECK(event);

  DataPipeControlMessage* data = static_cast<DataPipeControlMessage*>(
      event->GetMessage<UserMessageImpl>()->user_payload());
  data->command = command;
  data->num_bytes = num_bytes;

  int rv = node_controller->SendUserMessage(port, std::move(event));
  if (rv != ports::OK && rv != ports::ERROR_PORT_PEER_CLOSED) {
    DLOG(ERROR) << "Unexpected failure sending data pipe control message: "
                << rv;
  }
}

}  // namespace core
}  // namespace mojo_legacy
