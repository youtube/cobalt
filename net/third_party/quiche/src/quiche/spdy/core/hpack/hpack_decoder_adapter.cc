// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/spdy/core/hpack/hpack_decoder_adapter.h"

#include "quiche/http2/decoder/decode_buffer.h"
#include "quiche/http2/decoder/decode_status.h"
#include "quiche/common/platform/api/quiche_logging.h"

namespace spdy {
namespace {
const size_t kMaxDecodeBufferSizeBytes = 32 * 1024;  // 32 KB
}  // namespace

HpackDecoderAdapter::HpackDecoderAdapter()
    : hpack_decoder_(&listener_adapter_, kMaxDecodeBufferSizeBytes),
      max_decode_buffer_size_bytes_(kMaxDecodeBufferSizeBytes),
      max_header_block_bytes_(0),
      header_block_started_(false),
      error_(http2::HpackDecodingError::kOk) {}

HpackDecoderAdapter::~HpackDecoderAdapter() = default;

void HpackDecoderAdapter::ApplyHeaderTableSizeSetting(size_t size_setting) {
  QUICHE_DVLOG(2) << "HpackDecoderAdapter::ApplyHeaderTableSizeSetting";
  hpack_decoder_.ApplyHeaderTableSizeSetting(size_setting);
}

size_t HpackDecoderAdapter::GetCurrentHeaderTableSizeSetting() const {
  return hpack_decoder_.GetCurrentHeaderTableSizeSetting();
}

void HpackDecoderAdapter::HandleControlFrameHeadersStart(
    SpdyHeadersHandlerInterface* handler) {
  QUICHE_DVLOG(2) << "HpackDecoderAdapter::HandleControlFrameHeadersStart";
  QUICHE_DCHECK(!header_block_started_);
  listener_adapter_.set_handler(handler);
}

bool HpackDecoderAdapter::HandleControlFrameHeadersData(
    const char* headers_data, size_t headers_data_length) {
  QUICHE_DVLOG(2) << "HpackDecoderAdapter::HandleControlFrameHeadersData: len="
                  << headers_data_length;
  if (!header_block_started_) {
    // Initialize the decoding process here rather than in
    // HandleControlFrameHeadersStart because that method is not always called.
    header_block_started_ = true;
    if (!hpack_decoder_.StartDecodingBlock()) {
      header_block_started_ = false;
      error_ = hpack_decoder_.error();
      detailed_error_ = hpack_decoder_.detailed_error();
      return false;
    }
  }

  // Sometimes we get a call with headers_data==nullptr and
  // headers_data_length==0, in which case we need to avoid creating
  // a DecodeBuffer, which would otherwise complain.
  if (headers_data_length > 0) {
    QUICHE_DCHECK_NE(headers_data, nullptr);
    if (headers_data_length > max_decode_buffer_size_bytes_) {
      QUICHE_DVLOG(1) << "max_decode_buffer_size_bytes_ < headers_data_length: "
                      << max_decode_buffer_size_bytes_ << " < "
                      << headers_data_length;
      error_ = http2::HpackDecodingError::kFragmentTooLong;
      detailed_error_ = "";
      return false;
    }
    listener_adapter_.AddToTotalHpackBytes(headers_data_length);
    if (max_header_block_bytes_ != 0 &&
        listener_adapter_.total_hpack_bytes() > max_header_block_bytes_) {
      error_ = http2::HpackDecodingError::kCompressedHeaderSizeExceedsLimit;
      detailed_error_ = "";
      return false;
    }
    http2::DecodeBuffer db(headers_data, headers_data_length);
    bool ok = hpack_decoder_.DecodeFragment(&db);
    QUICHE_DCHECK(!ok || db.Empty()) << "Remaining=" << db.Remaining();
    if (!ok) {
      error_ = hpack_decoder_.error();
      detailed_error_ = hpack_decoder_.detailed_error();
    }
    return ok;
  }
  return true;
}

bool HpackDecoderAdapter::HandleControlFrameHeadersComplete() {
  QUICHE_DVLOG(2) << "HpackDecoderAdapter::HandleControlFrameHeadersComplete";
  if (!hpack_decoder_.EndDecodingBlock()) {
    QUICHE_DVLOG(3) << "EndDecodingBlock returned false";
    error_ = hpack_decoder_.error();
    detailed_error_ = hpack_decoder_.detailed_error();
    return false;
  }
  header_block_started_ = false;
  return true;
}

const Http2HeaderBlock& HpackDecoderAdapter::decoded_block() const {
  return listener_adapter_.decoded_block();
}

void HpackDecoderAdapter::set_max_decode_buffer_size_bytes(
    size_t max_decode_buffer_size_bytes) {
  QUICHE_DVLOG(2) << "HpackDecoderAdapter::set_max_decode_buffer_size_bytes";
  max_decode_buffer_size_bytes_ = max_decode_buffer_size_bytes;
  hpack_decoder_.set_max_string_size_bytes(max_decode_buffer_size_bytes);
}

void HpackDecoderAdapter::set_max_header_block_bytes(
    size_t max_header_block_bytes) {
  max_header_block_bytes_ = max_header_block_bytes;
}

HpackDecoderAdapter::ListenerAdapter::ListenerAdapter() : handler_(nullptr) {}
HpackDecoderAdapter::ListenerAdapter::~ListenerAdapter() = default;

void HpackDecoderAdapter::ListenerAdapter::set_handler(
    SpdyHeadersHandlerInterface* handler) {
  handler_ = handler;
}

void HpackDecoderAdapter::ListenerAdapter::OnHeaderListStart() {
  QUICHE_DVLOG(2) << "HpackDecoderAdapter::ListenerAdapter::OnHeaderListStart";
  total_hpack_bytes_ = 0;
  total_uncompressed_bytes_ = 0;
  decoded_block_.clear();
  if (handler_ != nullptr) {
    handler_->OnHeaderBlockStart();
  }
}

void HpackDecoderAdapter::ListenerAdapter::OnHeader(const std::string& name,
                                                    const std::string& value) {
  QUICHE_DVLOG(2) << "HpackDecoderAdapter::ListenerAdapter::OnHeader:\n name: "
                  << name << "\n value: " << value;
  total_uncompressed_bytes_ += name.size() + value.size();
  if (handler_ == nullptr) {
    QUICHE_DVLOG(3) << "Adding to decoded_block";
    decoded_block_.AppendValueOrAddHeader(name, value);
  } else {
    QUICHE_DVLOG(3) << "Passing to handler";
    handler_->OnHeader(name, value);
  }
}

void HpackDecoderAdapter::ListenerAdapter::OnHeaderListEnd() {
  QUICHE_DVLOG(2) << "HpackDecoderAdapter::ListenerAdapter::OnHeaderListEnd";
  // We don't clear the Http2HeaderBlock here to allow access to it until the
  // next HPACK block is decoded.
  if (handler_ != nullptr) {
    handler_->OnHeaderBlockEnd(total_uncompressed_bytes_, total_hpack_bytes_);
    handler_ = nullptr;
  }
}

void HpackDecoderAdapter::ListenerAdapter::OnHeaderErrorDetected(
    absl::string_view error_message) {
  QUICHE_VLOG(1) << error_message;
}

}  // namespace spdy
