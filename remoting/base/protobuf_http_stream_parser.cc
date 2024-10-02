// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/protobuf_http_stream_parser.h"

#include <string.h>

#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "remoting/base/protobuf_http_client_messages.pb.h"
#include "remoting/base/protobuf_http_status.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/wire_format_lite.h"

namespace remoting {

namespace {

using ::google::protobuf::internal::WireFormatLite;

constexpr int kReadBufferSpareCapacity = 512;

}  // namespace

ProtobufHttpStreamParser::ProtobufHttpStreamParser(
    const MessageCallback& message_callback,
    StreamClosedCallback stream_closed_callback)
    : message_callback_(message_callback),
      stream_closed_callback_(std::move(stream_closed_callback)) {
  DCHECK(message_callback_);
  DCHECK(stream_closed_callback_);
}

ProtobufHttpStreamParser::~ProtobufHttpStreamParser() = default;

void ProtobufHttpStreamParser::Append(base::StringPiece data) {
  int required_remaining_capacity = data.size() + kReadBufferSpareCapacity;
  if (!read_buffer_) {
    read_buffer_ = base::MakeRefCounted<net::GrowableIOBuffer>();
    read_buffer_->SetCapacity(required_remaining_capacity);
  } else if (read_buffer_->RemainingCapacity() < required_remaining_capacity) {
    read_buffer_->SetCapacity(read_buffer_->offset() +
                              required_remaining_capacity);
  }

  DCHECK_GE(read_buffer_->RemainingCapacity(), static_cast<int>(data.size()));
  memcpy(read_buffer_->data(), data.data(), data.size());
  read_buffer_->set_offset(read_buffer_->offset() + data.size());

  ParseStreamIfAvailable();
}

bool ProtobufHttpStreamParser::HasPendingData() const {
  return read_buffer_ && read_buffer_->offset() > 0;
}

void ProtobufHttpStreamParser::ParseStreamIfAvailable() {
  DCHECK(read_buffer_);

  google::protobuf::io::CodedInputStream input_stream(
      reinterpret_cast<const uint8_t*>(read_buffer_->StartOfBuffer()),
      read_buffer_->offset());
  int bytes_consumed = 0;
  auto weak_this = weak_factory_.GetWeakPtr();
  // We can't use StreamBody::ParseFromString() here, as it can't do partial
  // parsing, nor can it tell how many bytes are consumed.
  while (bytes_consumed < read_buffer_->offset()) {
    bool is_successful = ParseOneField(&input_stream);
    if (!weak_this) {
      // The callback might have deleted |this|, in which case we need to
      // carefully return without touching any member of |this|.
      return;
    }
    if (is_successful) {
      // Only update |bytes_consumed| if the whole field is decoded.
      // |input_stream| can still advance when the field is not decodable.
      bytes_consumed = input_stream.CurrentPosition();
    } else {
      // The stream data can't be fully decoded yet.
      break;
    }
  }

  if (bytes_consumed == 0) {
    return;
  }
  CHECK_LE(bytes_consumed, read_buffer_->offset());
  int bytes_not_consumed = read_buffer_->offset() - bytes_consumed;
  memmove(read_buffer_->StartOfBuffer(),
          read_buffer_->StartOfBuffer() + bytes_consumed, bytes_not_consumed);
  read_buffer_->set_offset(bytes_not_consumed);
}

bool ProtobufHttpStreamParser::ParseOneField(
    google::protobuf::io::CodedInputStream* input_stream) {
  // Note that the StreamBody definition is only significant in its tag ID
  // allocations for "messages" and "status". There isn't any clear boundary
  // between two StreamBody instances.
  //
  // A typical stream looks like:
  //
  //   [message tag] <length> <message> [message tag] <length> <message> ...
  //   [status tag] <status> EOF
  //
  // Stream data failing to comply with this format usually means more data is
  // needed.

  uint32_t message_tag = input_stream->ReadTag();
  if (message_tag == 0) {
    VLOG(1) << "Can't read message tag yet.";
    return false;
  }

  WireFormatLite::WireType wire_type =
      WireFormatLite::GetTagWireType(message_tag);
  int field_number = WireFormatLite::GetTagFieldNumber(message_tag);
  switch (field_number) {
    case protobufhttpclient::StreamBody::kMessagesFieldNumber: {
      DCHECK_EQ(WireFormatLite::WireType::WIRETYPE_LENGTH_DELIMITED, wire_type);
      std::string message;
      if (!WireFormatLite::ReadBytes(input_stream, &message)) {
        VLOG(1) << "Can't read stream message yet.";
        return false;
      }
      VLOG(1) << "Stream message decoded.";
      message_callback_.Run(message);
      break;
    }

    case protobufhttpclient::StreamBody::kStatusFieldNumber: {
      DCHECK_EQ(WireFormatLite::WireType::WIRETYPE_LENGTH_DELIMITED, wire_type);
      protobufhttpclient::Status status;
      if (!WireFormatLite::ReadMessage(input_stream, &status)) {
        VLOG(1) << "Can't read status yet.";
        return false;
      }
      VLOG(1) << "Client status decoded.";
      std::move(stream_closed_callback_).Run(ProtobufHttpStatus(status));
      break;
    }

    default:
      if (field_number == protobufhttpclient::StreamBody::kNoopFieldNumber) {
        VLOG(1) << "Found noop field.";
      } else {
        LOG(WARNING) << "Skipping unrecognized StreamBody field: "
                     << field_number
                     << ", wire type: " << static_cast<int>(wire_type);
      }
      if (!WireFormatLite::SkipField(input_stream, message_tag)) {
        VLOG(1) << "Can't skip the field yet.";
        return false;
      }
      break;
  }
  return true;
}

}  // namespace remoting
