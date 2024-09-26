// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dawn_service_serializer.h"

#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/common/webgpu_cmd_format.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "ipc/ipc_channel.h"

namespace gpu::webgpu {

namespace {

constexpr size_t kMaxWireBufferSize =
    std::min(IPC::Channel::kMaximumMessageSize,
             static_cast<size_t>(1024 * 1024));

constexpr size_t kDawnReturnCmdsOffset =
    offsetof(cmds::DawnReturnCommandsInfo, deserialized_buffer);

static_assert(kDawnReturnCmdsOffset < kMaxWireBufferSize, "");

}  // anonymous namespace

DawnServiceSerializer::DawnServiceSerializer(DecoderClient* client)
    : client_(client),
      buffer_(kMaxWireBufferSize),
      put_offset_(offsetof(cmds::DawnReturnCommandsInfo, deserialized_buffer)) {
  // We prepopulate the message with the header and keep it between flushes so
  // we never need to write it again.
  cmds::DawnReturnCommandsInfoHeader* header =
      reinterpret_cast<cmds::DawnReturnCommandsInfoHeader*>(&buffer_[0]);
  header->return_data_header.return_data_type =
      DawnReturnDataType::kDawnCommands;
}

DawnServiceSerializer::~DawnServiceSerializer() = default;

size_t DawnServiceSerializer::GetMaximumAllocationSize() const {
  return kMaxWireBufferSize - kDawnReturnCmdsOffset;
}

void* DawnServiceSerializer::GetCmdSpace(size_t size) {
  // Note: Dawn will never call this function with |size| >
  // GetMaximumAllocationSize().
  DCHECK_LE(put_offset_, kMaxWireBufferSize);
  DCHECK_LE(size, GetMaximumAllocationSize());

  // Statically check that kMaxWireBufferSize + kMaxWireBufferSize is
  // a valid uint32_t. We can add put_offset_ and size without overflow.
  static_assert(base::CheckAdd(kMaxWireBufferSize, kMaxWireBufferSize)
                    .IsValid<uint32_t>(),
                "");
  uint32_t next_offset = put_offset_ + static_cast<uint32_t>(size);
  if (next_offset > buffer_.size()) {
    Flush();
    // TODO(enga): Keep track of how much command space the application is using
    // and adjust the buffer size accordingly.

    DCHECK_EQ(put_offset_, kDawnReturnCmdsOffset);
    next_offset = put_offset_ + static_cast<uint32_t>(size);
  }

  uint8_t* ptr = &buffer_[put_offset_];
  put_offset_ = next_offset;
  return ptr;
}

bool DawnServiceSerializer::NeedsFlush() const {
  return put_offset_ > kDawnReturnCmdsOffset;
}

bool DawnServiceSerializer::Flush() {
  if (NeedsFlush()) {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
                 "DawnServiceSerializer::Flush", "bytes", put_offset_);

    static uint32_t return_trace_id = 0;
    TRACE_EVENT_WITH_FLOW0(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
                           "DawnReturnCommands", return_trace_id++,
                           TRACE_EVENT_FLAG_FLOW_OUT);

    client_->HandleReturnData(base::make_span(buffer_.data(), put_offset_));
    put_offset_ = kDawnReturnCmdsOffset;
  }
  return true;
}

}  //  namespace gpu::webgpu