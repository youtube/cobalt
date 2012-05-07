// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(rtenhove) clean up frame buffer size calculations so that we aren't
// constantly adding and subtracting header sizes; this is ugly and error-
// prone.

#include "net/spdy/spdy_framer.h"

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/stats_counters.h"
#include "base/third_party/valgrind/memcheck.h"
#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_frame_reader.h"
#include "net/spdy/spdy_bitmasks.h"

#if defined(USE_SYSTEM_ZLIB)
#include <zlib.h>
#else
#include "third_party/zlib/zlib.h"
#endif

using std::vector;

namespace net {

namespace {

// Compute the id of our dictionary so that we know we're using the
// right one when asked for it.
uLong CalculateDictionaryId(const char* dictionary,
                            const size_t dictionary_size) {
  uLong initial_value = adler32(0L, Z_NULL, 0);
  return adler32(initial_value,
                 reinterpret_cast<const Bytef*>(dictionary),
                 dictionary_size);
}

struct DictionaryIds {
  DictionaryIds()
    : v2_dictionary_id(CalculateDictionaryId(kV2Dictionary, kV2DictionarySize)),
      v3_dictionary_id(CalculateDictionaryId(kV3Dictionary, kV3DictionarySize))
  {}
  const uLong v2_dictionary_id;
  const uLong v3_dictionary_id;
};

// Adler ID for the SPDY header compressor dictionaries. Note that they are
// initialized lazily to avoid static initializers.
base::LazyInstance<DictionaryIds>::Leaky g_dictionary_ids;

}  // namespace

const int SpdyFramer::kMinSpdyVersion = 2;
const int SpdyFramer::kMaxSpdyVersion = 3;
const SpdyStreamId SpdyFramer::kInvalidStream = -1;
const size_t SpdyFramer::kHeaderDataChunkMaxSize = 1024;
const size_t SpdyFramer::kControlFrameBufferSize =
    sizeof(SpdySynStreamControlFrameBlock);
const size_t SpdyFramer::kMaxControlFrameSize = 16 * 1024;

#ifdef DEBUG_SPDY_STATE_CHANGES
#define CHANGE_STATE(newstate)                                  \
  do {                                                          \
    LOG(INFO) << "Changing state from: "                        \
              << StateToString(state_)                          \
              << " to " << StateToString(newstate) << "\n";     \
    DCHECK(state_ != SPDY_ERROR);                               \
    DCHECK_EQ(previous_state_, state_);                         \
    previous_state_ = state_;                                   \
    state_ = newstate;                                          \
  } while (false)
#else
#define CHANGE_STATE(newstate)                                  \
  do {                                                          \
    DCHECK(state_ != SPDY_ERROR);                               \
    DCHECK_EQ(previous_state_, state_);                         \
    previous_state_ = state_;                                   \
    state_ = newstate;                                          \
  } while (false)
#endif

SettingsFlagsAndId SettingsFlagsAndId::FromWireFormat(int version,
                                                      uint32 wire) {
  if (version < 3) {
    ConvertFlagsAndIdForSpdy2(&wire);
  }
  return SettingsFlagsAndId(ntohl(wire) >> 24, ntohl(wire) & 0x00ffffff);
}

SettingsFlagsAndId::SettingsFlagsAndId(uint8 flags, uint32 id)
    : flags_(flags), id_(id & 0x00ffffff) {
  DCHECK_GT(1u << 24, id) << "SPDY setting ID too large.";
}

uint32 SettingsFlagsAndId::GetWireFormat(int version) const {
  uint32 wire = htonl(id_ & 0x00ffffff) | htonl(flags_ << 24);
  if (version < 3) {
    ConvertFlagsAndIdForSpdy2(&wire);
  }
  return wire;
}

// SPDY 2 had a bug in it with respect to byte ordering of id/flags field.
// This method is used to preserve buggy behavior and works on both
// little-endian and big-endian hosts.
// This method is also bidirectional (can be used to translate SPDY 2 to SPDY 3
// as well as vice versa).
void SettingsFlagsAndId::ConvertFlagsAndIdForSpdy2(uint32* val) {
    uint8* wire_array = reinterpret_cast<uint8*>(val);
    std::swap(wire_array[0], wire_array[3]);
    std::swap(wire_array[1], wire_array[2]);
}

SpdyCredential::SpdyCredential() : slot(0) {}
SpdyCredential::~SpdyCredential() {}

SpdyFramer::SpdyFramer(int version)
    : state_(SPDY_RESET),
      previous_state_(SPDY_RESET),
      error_code_(SPDY_NO_ERROR),
      remaining_data_(0),
      remaining_control_payload_(0),
      remaining_control_header_(0),
      current_frame_buffer_(new char[kControlFrameBufferSize]),
      current_frame_len_(0),
      enable_compression_(true),
      visitor_(NULL),
      display_protocol_("SPDY"),
      spdy_version_(version),
      syn_frame_processed_(false),
      probable_http_response_(false) {
  DCHECK_GE(kMaxSpdyVersion, version);
  DCHECK_LE(kMinSpdyVersion, version);
}

SpdyFramer::~SpdyFramer() {
  if (header_compressor_.get()) {
    deflateEnd(header_compressor_.get());
  }
  if (header_decompressor_.get()) {
    inflateEnd(header_decompressor_.get());
  }
}

void SpdyFramer::Reset() {
  state_ = SPDY_RESET;
  previous_state_ = SPDY_RESET;
  error_code_ = SPDY_NO_ERROR;
  remaining_data_ = 0;
  remaining_control_payload_ = 0;
  remaining_control_header_ = 0;
  current_frame_len_ = 0;
  settings_scratch_.Reset();
}

const char* SpdyFramer::StateToString(int state) {
  switch (state) {
    case SPDY_ERROR:
      return "ERROR";
    case SPDY_DONE:
      return "DONE";
    case SPDY_AUTO_RESET:
      return "AUTO_RESET";
    case SPDY_RESET:
      return "RESET";
    case SPDY_READING_COMMON_HEADER:
      return "READING_COMMON_HEADER";
    case SPDY_CONTROL_FRAME_PAYLOAD:
      return "CONTROL_FRAME_PAYLOAD";
    case SPDY_IGNORE_REMAINING_PAYLOAD:
      return "IGNORE_REMAINING_PAYLOAD";
    case SPDY_FORWARD_STREAM_FRAME:
      return "FORWARD_STREAM_FRAME";
    case SPDY_CONTROL_FRAME_BEFORE_HEADER_BLOCK:
      return "SPDY_CONTROL_FRAME_BEFORE_HEADER_BLOCK";
    case SPDY_CONTROL_FRAME_HEADER_BLOCK:
      return "SPDY_CONTROL_FRAME_HEADER_BLOCK";
    case SPDY_CREDENTIAL_FRAME_PAYLOAD:
      return "SPDY_CREDENTIAL_FRAME_PAYLOAD";
    case SPDY_SETTINGS_FRAME_PAYLOAD:
      return "SPDY_SETTINGS_FRAME_PAYLOAD";
  }
  return "UNKNOWN_STATE";
}

void SpdyFramer::set_error(SpdyError error) {
  DCHECK(visitor_);
  error_code_ = error;
  CHANGE_STATE(SPDY_ERROR);
  visitor_->OnError(this);
}

const char* SpdyFramer::ErrorCodeToString(int error_code) {
  switch (error_code) {
    case SPDY_NO_ERROR:
      return "NO_ERROR";
    case SPDY_INVALID_CONTROL_FRAME:
      return "INVALID_CONTROL_FRAME";
    case SPDY_CONTROL_PAYLOAD_TOO_LARGE:
      return "CONTROL_PAYLOAD_TOO_LARGE";
    case SPDY_ZLIB_INIT_FAILURE:
      return "ZLIB_INIT_FAILURE";
    case SPDY_UNSUPPORTED_VERSION:
      return "UNSUPPORTED_VERSION";
    case SPDY_DECOMPRESS_FAILURE:
      return "DECOMPRESS_FAILURE";
    case SPDY_COMPRESS_FAILURE:
      return "COMPRESS_FAILURE";
    case SPDY_INVALID_DATA_FRAME_FLAGS:
      return "SPDY_INVALID_DATA_FRAME_FLAGS";
  }
  return "UNKNOWN_ERROR";
}

const char* SpdyFramer::StatusCodeToString(int status_code) {
  switch (status_code) {
    case INVALID:
      return "INVALID";
    case PROTOCOL_ERROR:
      return "PROTOCOL_ERROR";
    case INVALID_STREAM:
      return "INVALID_STREAM";
    case REFUSED_STREAM:
      return "REFUSED_STREAM";
    case UNSUPPORTED_VERSION:
      return "UNSUPPORTED_VERSION";
    case CANCEL:
      return "CANCEL";
    case INTERNAL_ERROR:
      return "INTERNAL_ERROR";
    case FLOW_CONTROL_ERROR:
      return "FLOW_CONTROL_ERROR";
    case STREAM_IN_USE:
      return "STREAM_IN_USE";
    case STREAM_ALREADY_CLOSED:
      return "STREAM_ALREADY_CLOSED";
    case INVALID_CREDENTIALS:
      return "INVALID_CREDENTIALS";
    case FRAME_TOO_LARGE:
      return "FRAME_TOO_LARGE";
  }
  return "UNKNOWN_STATUS";
}

const char* SpdyFramer::ControlTypeToString(SpdyControlType type) {
  switch (type) {
    case SYN_STREAM:
      return "SYN_STREAM";
    case SYN_REPLY:
      return "SYN_REPLY";
    case RST_STREAM:
      return "RST_STREAM";
    case SETTINGS:
      return "SETTINGS";
    case NOOP:
      return "NOOP";
    case PING:
      return "PING";
    case GOAWAY:
      return "GOAWAY";
    case HEADERS:
      return "HEADERS";
    case WINDOW_UPDATE:
      return "WINDOW_UPDATE";
    case CREDENTIAL:
      return "CREDENTIAL";
    case NUM_CONTROL_FRAME_TYPES:
      break;
  }
  return "UNKNOWN_CONTROL_TYPE";
}

size_t SpdyFramer::ProcessInput(const char* data, size_t len) {
  DCHECK(visitor_);
  DCHECK(data);

  size_t original_len = len;
  do {
    previous_state_ = state_;
    switch (state_) {
      case SPDY_ERROR:
      case SPDY_DONE:
        goto bottom;

      case SPDY_AUTO_RESET:
      case SPDY_RESET:
        Reset();
        if (len > 0) {
          CHANGE_STATE(SPDY_READING_COMMON_HEADER);
        }
        break;

      case SPDY_READING_COMMON_HEADER: {
        size_t bytes_read = ProcessCommonHeader(data, len);
        len -= bytes_read;
        data += bytes_read;
        break;
      }

      case SPDY_CONTROL_FRAME_BEFORE_HEADER_BLOCK: {
        // Control frames that contain header blocks (SYN_STREAM, SYN_REPLY,
        // HEADERS) take a different path through the state machine - they
        // will go:
        //   1. SPDY_CONTROL_FRAME_BEFORE_HEADER_BLOCK
        //   2. SPDY_CONTROL_FRAME_HEADER_BLOCK
        //
        // SETTINGS frames take a slightly modified route:
        //   1. SPDY_CONTROL_FRAME_BEFORE_HEADER_BLOCK
        //   2. SPDY_SETTINGS_FRAME_PAYLOAD
        //
        //  All other control frames will use the alternate route directly to
        //  SPDY_CONTROL_FRAME_PAYLOAD
        int bytes_read = ProcessControlFrameBeforeHeaderBlock(data, len);
        len -= bytes_read;
        data += bytes_read;
        break;
      }

      case SPDY_SETTINGS_FRAME_PAYLOAD: {
        int bytes_read = ProcessSettingsFramePayload(data, len);
        len -= bytes_read;
        data += bytes_read;
        break;
      }

      case SPDY_CONTROL_FRAME_HEADER_BLOCK: {
        int bytes_read = ProcessControlFrameHeaderBlock(data, len);
        len -= bytes_read;
        data += bytes_read;
        break;
      }

      case SPDY_CREDENTIAL_FRAME_PAYLOAD: {
        size_t bytes_read = ProcessCredentialFramePayload(data, len);
        len -= bytes_read;
        data += bytes_read;
        break;
      }

      case SPDY_CONTROL_FRAME_PAYLOAD: {
        size_t bytes_read = ProcessControlFramePayload(data, len);
        len -= bytes_read;
        data += bytes_read;
        break;
      }

      case SPDY_IGNORE_REMAINING_PAYLOAD:
        // control frame has too-large payload
        // intentional fallthrough
      case SPDY_FORWARD_STREAM_FRAME: {
        size_t bytes_read = ProcessDataFramePayload(data, len);
        len -= bytes_read;
        data += bytes_read;
        break;
      }
      default:
        LOG(DFATAL) << "Invalid value for " << display_protocol_
                    << " framer state: " << state_;
        // This ensures that we don't infinite-loop if state_ gets an
        // invalid value somehow, such as due to a SpdyFramer getting deleted
        // from a callback it calls.
        goto bottom;
    }
  } while (state_ != previous_state_);
 bottom:
  DCHECK(len == 0 || state_ == SPDY_ERROR);
  if (current_frame_len_ == 0 &&
      remaining_data_ == 0 &&
      remaining_control_payload_ == 0 &&
      remaining_control_header_ == 0) {
    DCHECK(state_ == SPDY_RESET || state_ == SPDY_ERROR)
        << "State: " << StateToString(state_);
  }

  return original_len - len;
}

size_t SpdyFramer::ProcessCommonHeader(const char* data, size_t len) {
  // This should only be called when we're in the SPDY_READING_COMMON_HEADER
  // state.
  DCHECK_EQ(state_, SPDY_READING_COMMON_HEADER);

  size_t original_len = len;
  SpdyFrame current_frame(current_frame_buffer_.get(), false);

  // Update current frame buffer as needed.
  if (current_frame_len_ < SpdyFrame::kHeaderSize) {
    size_t bytes_desired = SpdyFrame::kHeaderSize - current_frame_len_;
    UpdateCurrentFrameBuffer(&data, &len, bytes_desired);
  }

  if (current_frame_len_ < SpdyFrame::kHeaderSize) {
    // TODO(rch): remove this empty block
    // Do nothing.
  } else {
    remaining_data_ = current_frame.length();

    // This is just a sanity check for help debugging early frame errors.
    if (remaining_data_ > 1000000u) {
      // The strncmp for 5 is safe because we only hit this point if we
      // have SpdyFrame::kHeaderSize (8) bytes
      if (!syn_frame_processed_ &&
          strncmp(current_frame_buffer_.get(), "HTTP/", 5) == 0) {
        LOG(WARNING) << "Unexpected HTTP response to spdy request";
        probable_http_response_ = true;
      } else {
        LOG(WARNING) << "Unexpectedly large frame.  " << display_protocol_
                     << " session is likely corrupt.";
      }
    }

    // if we're here, then we have the common header all received.
    if (!current_frame.is_control_frame()) {
      SpdyDataFrame data_frame(current_frame_buffer_.get(), false);
      visitor_->OnDataFrameHeader(&data_frame);

      if (current_frame.length() > 0) {
        CHANGE_STATE(SPDY_FORWARD_STREAM_FRAME);
      } else {
        // Empty data frame.
        if (current_frame.flags() & DATA_FLAG_FIN) {
          visitor_->OnStreamFrameData(data_frame.stream_id(), NULL, 0);
        }
        CHANGE_STATE(SPDY_AUTO_RESET);
      }
    } else {
      ProcessControlFrameHeader();
    }
  }
  return original_len - len;
}

void SpdyFramer::ProcessControlFrameHeader() {
  DCHECK_EQ(SPDY_NO_ERROR, error_code_);
  DCHECK_LE(static_cast<size_t>(SpdyFrame::kHeaderSize), current_frame_len_);
  SpdyControlFrame current_control_frame(current_frame_buffer_.get(), false);

  // We check version before we check validity: version can never be 'invalid',
  // it can only be unsupported.
  if (current_control_frame.version() != spdy_version_) {
    DLOG(INFO) << "Unsupported SPDY version " << current_control_frame.version()
               << " (expected " << spdy_version_ << ")";
    set_error(SPDY_UNSUPPORTED_VERSION);
    return;
  }

  // Next up, check to see if we have valid data.  This should be after version
  // checking (otherwise if the the type were out of bounds due to a version
  // upgrade we would misclassify the error) and before checking the type
  // (type can definitely be out of bounds)
  if (!current_control_frame.AppearsToBeAValidControlFrame()) {
    set_error(SPDY_INVALID_CONTROL_FRAME);
    return;
  }

  if (current_control_frame.type() == NOOP) {
    DLOG(INFO) << "NOOP control frame found. Ignoring.";
    CHANGE_STATE(SPDY_AUTO_RESET);
    return;
  }

  // Do some sanity checking on the control frame sizes.
  switch (current_control_frame.type()) {
    case SYN_STREAM:
      if (current_control_frame.length() <
          SpdySynStreamControlFrame::size() - SpdyControlFrame::kHeaderSize)
        set_error(SPDY_INVALID_CONTROL_FRAME);
      break;
    case SYN_REPLY:
      if (current_control_frame.length() <
          SpdySynReplyControlFrame::size() - SpdyControlFrame::kHeaderSize)
        set_error(SPDY_INVALID_CONTROL_FRAME);
      break;
    case RST_STREAM:
      if (current_control_frame.length() !=
          SpdyRstStreamControlFrame::size() - SpdyFrame::kHeaderSize)
        set_error(SPDY_INVALID_CONTROL_FRAME);
      break;
    case SETTINGS:
      // Make sure that we have an integral number of 8-byte key/value pairs,
      // plus a 4-byte length field.
      if (current_control_frame.length() <
          SpdySettingsControlFrame::size() - SpdyControlFrame::kHeaderSize ||
          (current_control_frame.length() % 8 != 4)) {
        DLOG(WARNING) << "Invalid length for SETTINGS frame: "
                      << current_control_frame.length();
        set_error(SPDY_INVALID_CONTROL_FRAME);
      }
      break;
    case GOAWAY:
      {
        // SPDY 2 GOAWAY frames are 4 bytes smaller than in SPDY 3. We account
        // for this difference via a separate offset variable, since
        // SpdyGoAwayControlFrame::size() returns the SPDY 3 size.
        const size_t goaway_offset = (protocol_version() < 3) ? 4 : 0;
        if (current_control_frame.length() + goaway_offset !=
            SpdyGoAwayControlFrame::size() - SpdyFrame::kHeaderSize)
          set_error(SPDY_INVALID_CONTROL_FRAME);
        break;
      }
    case HEADERS:
      if (current_control_frame.length() <
          SpdyHeadersControlFrame::size() - SpdyControlFrame::kHeaderSize)
        set_error(SPDY_INVALID_CONTROL_FRAME);
      break;
    case WINDOW_UPDATE:
      if (current_control_frame.length() !=
          SpdyWindowUpdateControlFrame::size() -
          SpdyControlFrame::kHeaderSize)
        set_error(SPDY_INVALID_CONTROL_FRAME);
      break;
    case PING:
      if (current_control_frame.length() !=
          SpdyPingControlFrame::size() - SpdyControlFrame::kHeaderSize)
        set_error(SPDY_INVALID_CONTROL_FRAME);
      break;
    case CREDENTIAL:
      if (current_control_frame.length() <
          SpdyCredentialControlFrame::size() - SpdyControlFrame::kHeaderSize)
        set_error(SPDY_INVALID_CONTROL_FRAME);
      break;
    default:
      LOG(WARNING) << "Valid " << display_protocol_
                   << " control frame with unhandled type: "
                   << current_control_frame.type();
      DLOG(FATAL);
      set_error(SPDY_INVALID_CONTROL_FRAME);
      break;
  }

  if (state_ == SPDY_ERROR) {
    return;
  }

  remaining_control_payload_ = current_control_frame.length();
  const size_t total_frame_size =
      remaining_control_payload_ + SpdyFrame::kHeaderSize;
  if (total_frame_size > kMaxControlFrameSize) {
    DLOG(WARNING) << "Received control frame with way too big of a payload: "
                  << total_frame_size;
    set_error(SPDY_CONTROL_PAYLOAD_TOO_LARGE);
    return;
  }

  if (current_control_frame.type() == CREDENTIAL) {
    visitor_->OnControl(&current_control_frame);
    CHANGE_STATE(SPDY_CREDENTIAL_FRAME_PAYLOAD);
    return;
  }

  // Determine the frame size without variable-length data.
  int32 frame_size_without_variable_data;
  switch (current_control_frame.type()) {
    case SYN_STREAM:
      syn_frame_processed_ = true;
      frame_size_without_variable_data = SpdySynStreamControlFrame::size();
      break;
    case SYN_REPLY:
      syn_frame_processed_ = true;
      frame_size_without_variable_data = SpdySynReplyControlFrame::size();
      // SPDY 2 had two bytes of unused space preceeding payload.
      if (spdy_version_ < 3) {
        frame_size_without_variable_data += 2;
      }
      break;
    case HEADERS:
      frame_size_without_variable_data = SpdyHeadersControlFrame::size();
      // SPDY 2 had two bytes of unused space preceeding payload.
      if (spdy_version_ < 3) {
        frame_size_without_variable_data += 2;
      }
      break;
    case SETTINGS:
      frame_size_without_variable_data = SpdySettingsControlFrame::size();
      break;
    default:
      frame_size_without_variable_data = -1;
      break;
  }

  if ((frame_size_without_variable_data == -1) &&
      (total_frame_size > kControlFrameBufferSize)) {
    // We should already be in an error state. Double-check.
    DCHECK_EQ(SPDY_ERROR, state_);
    if (state_ != SPDY_ERROR) {
      LOG(DFATAL) << display_protocol_
                  << " control frame buffer too small for fixed-length frame.";
      set_error(SPDY_CONTROL_PAYLOAD_TOO_LARGE);
    }
    return;
  }
  if (frame_size_without_variable_data > 0) {
    // We have a control frame with a header block. We need to parse the
    // remainder of the control frame's header before we can parse the header
    // block. The start of the header block varies with the control type.
    DCHECK_GE(frame_size_without_variable_data,
              static_cast<int32>(current_frame_len_));
    remaining_control_header_ = frame_size_without_variable_data -
        current_frame_len_;
    remaining_control_payload_ += SpdyFrame::kHeaderSize -
        frame_size_without_variable_data;
    CHANGE_STATE(SPDY_CONTROL_FRAME_BEFORE_HEADER_BLOCK);
    return;
  }

  CHANGE_STATE(SPDY_CONTROL_FRAME_PAYLOAD);
}

size_t SpdyFramer::UpdateCurrentFrameBuffer(const char** data, size_t* len,
                                            size_t max_bytes) {
  size_t bytes_to_read = std::min(*len, max_bytes);
  DCHECK_GE(kControlFrameBufferSize, current_frame_len_ + bytes_to_read);
  memcpy(current_frame_buffer_.get() + current_frame_len_,
         *data,
         bytes_to_read);
  current_frame_len_ += bytes_to_read;
  *data += bytes_to_read;
  *len -= bytes_to_read;
  return bytes_to_read;
}

size_t SpdyFramer::GetSerializedLength(const SpdyHeaderBlock* headers) const {
  const size_t num_name_value_pairs_size
      = (spdy_version_ < 3) ? sizeof(uint16) : sizeof(uint32);
  const size_t length_of_name_size = num_name_value_pairs_size;
  const size_t length_of_value_size = num_name_value_pairs_size;

  size_t total_length = num_name_value_pairs_size;
  for (SpdyHeaderBlock::const_iterator it = headers->begin();
       it != headers->end();
       ++it) {
    // We add space for the length of the name and the length of the value as
    // well as the length of the name and the length of the value.
    total_length += length_of_name_size + it->first.size() +
                    length_of_value_size + it->second.size();
  }
  return total_length;
}

void SpdyFramer::WriteHeaderBlock(SpdyFrameBuilder* frame,
                                  const SpdyHeaderBlock* headers) const {
  if (spdy_version_ < 3) {
    frame->WriteUInt16(headers->size());  // Number of headers.
  } else {
    frame->WriteUInt32(headers->size());  // Number of headers.
  }
  SpdyHeaderBlock::const_iterator it;
  for (it = headers->begin(); it != headers->end(); ++it) {
    bool wrote_header;
    if (spdy_version_ < 3) {
      wrote_header = frame->WriteString(it->first);
      wrote_header &= frame->WriteString(it->second);
    } else {
      wrote_header = frame->WriteStringPiece32(it->first);
      wrote_header &= frame->WriteStringPiece32(it->second);
    }
    DCHECK(wrote_header);
  }
}


size_t SpdyFramer::ProcessControlFrameBeforeHeaderBlock(const char* data,
                                                        size_t len) {
  DCHECK_EQ(SPDY_CONTROL_FRAME_BEFORE_HEADER_BLOCK, state_);
  size_t original_len = len;

  if (remaining_control_header_ > 0) {
    size_t bytes_read = UpdateCurrentFrameBuffer(&data, &len,
                                                 remaining_control_header_);
    remaining_control_header_ -= bytes_read;
  }

  if (remaining_control_header_ == 0) {
    SpdyControlFrame control_frame(current_frame_buffer_.get(), false);
    DCHECK(control_frame.type() == SYN_STREAM ||
           control_frame.type() == SYN_REPLY ||
           control_frame.type() == HEADERS ||
           control_frame.type() == SETTINGS);
    visitor_->OnControl(&control_frame);

    if (control_frame.type() == SETTINGS) {
      CHANGE_STATE(SPDY_SETTINGS_FRAME_PAYLOAD);
    } else {
      CHANGE_STATE(SPDY_CONTROL_FRAME_HEADER_BLOCK);
    }
  }
  return original_len - len;
}

// Does not buffer the control payload. Instead, either passes directly to the
// visitor or decompresses and then passes directly to the visitor, via
// IncrementallyDeliverControlFrameHeaderData() or
// IncrementallyDecompressControlFrameHeaderData() respectively.
size_t SpdyFramer::ProcessControlFrameHeaderBlock(const char* data,
                                                  size_t data_len) {
  DCHECK_EQ(SPDY_CONTROL_FRAME_HEADER_BLOCK, state_);
  SpdyControlFrame control_frame(current_frame_buffer_.get(), false);

  bool processed_successfully = true;
  DCHECK(control_frame.type() == SYN_STREAM ||
         control_frame.type() == SYN_REPLY ||
         control_frame.type() == HEADERS);
  size_t process_bytes = std::min(data_len, remaining_control_payload_);
  if (process_bytes > 0) {
    if (enable_compression_) {
      processed_successfully = IncrementallyDecompressControlFrameHeaderData(
          &control_frame, data, process_bytes);
    } else {
      processed_successfully = IncrementallyDeliverControlFrameHeaderData(
          &control_frame, data, process_bytes);
    }

    remaining_control_payload_ -= process_bytes;
    remaining_data_ -= process_bytes;
  }

  // Handle the case that there is no futher data in this frame.
  if (remaining_control_payload_ == 0 && processed_successfully) {
    // The complete header block has been delivered. We send a zero-length
    // OnControlFrameHeaderData() to indicate this.
    visitor_->OnControlFrameHeaderData(
        GetControlFrameStreamId(&control_frame), NULL, 0);

    // If this is a FIN, tell the caller.
    if (control_frame.flags() & CONTROL_FLAG_FIN) {
      visitor_->OnStreamFrameData(GetControlFrameStreamId(&control_frame),
                                  NULL, 0);
    }

    CHANGE_STATE(SPDY_AUTO_RESET);
  }

  // Handle error.
  if (!processed_successfully) {
    return data_len;
  }

  // Return amount processed.
  return process_bytes;
}

size_t SpdyFramer::ProcessSettingsFramePayload(const char* data,
                                               size_t data_len) {
  DCHECK_EQ(SPDY_SETTINGS_FRAME_PAYLOAD, state_);
  SpdyControlFrame control_frame(current_frame_buffer_.get(), false);
  DCHECK_EQ(SETTINGS, control_frame.type());
  size_t unprocessed_bytes = std::min(data_len, remaining_control_payload_);
  size_t processed_bytes = 0;

  // Loop over our incoming data.
  while (unprocessed_bytes > 0) {
    // Process up to one setting at a time.
    size_t processing = std::min(
        unprocessed_bytes,
        static_cast<size_t>(8 - settings_scratch_.setting_buf_len));

    // Check if we have a complete setting in our input.
    if (processing == 8) {
      // Parse the setting directly out of the input without buffering.
      if (!ProcessSetting(data + processed_bytes)) {
        set_error(SPDY_INVALID_CONTROL_FRAME);
        return processed_bytes;
      }
    } else {
      // Continue updating settings_scratch_.setting_buf.
      memcpy(settings_scratch_.setting_buf + settings_scratch_.setting_buf_len,
             data + processed_bytes,
             processing);
      settings_scratch_.setting_buf_len += processing;

      // Check if we have a complete setting buffered.
      if (settings_scratch_.setting_buf_len == 8) {
        if (!ProcessSetting(settings_scratch_.setting_buf)) {
          set_error(SPDY_INVALID_CONTROL_FRAME);
          return processed_bytes;
        }
        // Reset settings_scratch_.setting_buf for our next setting.
        settings_scratch_.setting_buf_len = 0;
      }
    }

    // Iterate.
    unprocessed_bytes -= processing;
    processed_bytes += processing;
  }

  // Check if we're done handling this SETTINGS frame.
  remaining_control_payload_ -= processed_bytes;
  if (remaining_control_payload_ == 0) {
    CHANGE_STATE(SPDY_AUTO_RESET);
  }

  return processed_bytes;
}

bool SpdyFramer::ProcessSetting(const char* data) {
  // Extract fields.
  // Maintain behavior of old SPDY 2 bug with byte ordering of flags/id.
  const uint32 id_and_flags_wire = *(reinterpret_cast<const uint32*>(data));
  SettingsFlagsAndId id_and_flags =
      SettingsFlagsAndId::FromWireFormat(spdy_version_, id_and_flags_wire);
  uint8 flags = id_and_flags.flags();
  uint32 value = ntohl(*(reinterpret_cast<const uint32*>(data + 4)));

  // Validate id.
  switch (id_and_flags.id()) {
    case SETTINGS_UPLOAD_BANDWIDTH:
    case SETTINGS_DOWNLOAD_BANDWIDTH:
    case SETTINGS_ROUND_TRIP_TIME:
    case SETTINGS_MAX_CONCURRENT_STREAMS:
    case SETTINGS_CURRENT_CWND:
    case SETTINGS_DOWNLOAD_RETRANS_RATE:
    case SETTINGS_INITIAL_WINDOW_SIZE:
      // Valid values.
      break;
    default:
      DLOG(WARNING) << "Unknown SETTINGS ID: " << id_and_flags.id();
      return false;
  }
  SpdySettingsIds id = static_cast<SpdySettingsIds>(id_and_flags.id());

  // Detect duplciates.
  if (static_cast<uint32>(id) <= settings_scratch_.last_setting_id) {
    DLOG(WARNING) << "Duplicate entry or invalid ordering for id " << id
                  << " in " << display_protocol_ << " SETTINGS frame "
                  << "(last settikng id was "
                  << settings_scratch_.last_setting_id << ").";
    return false;
  }
  settings_scratch_.last_setting_id = id;

  // Validate flags.
  uint8 kFlagsMask = SETTINGS_FLAG_PLEASE_PERSIST | SETTINGS_FLAG_PERSISTED;
  if ((flags & ~(kFlagsMask)) != 0) {
    DLOG(WARNING) << "Unknown SETTINGS flags provided for id " << id << ": "
                  << flags;
    return false;
  }

  // Validation succeeded. Pass on to visitor.
  visitor_->OnSetting(id, flags, value);
  return true;
}

size_t SpdyFramer::ProcessControlFramePayload(const char* data, size_t len) {
  size_t original_len = len;
  if (remaining_control_payload_) {
    size_t bytes_read = UpdateCurrentFrameBuffer(&data, &len,
                                                 remaining_control_payload_);
    remaining_control_payload_ -= bytes_read;
    remaining_data_ -= bytes_read;
    if (remaining_control_payload_ == 0) {
      SpdyControlFrame control_frame(current_frame_buffer_.get(), false);
      DCHECK(!control_frame.has_header_block());
      visitor_->OnControl(&control_frame);

      CHANGE_STATE(SPDY_IGNORE_REMAINING_PAYLOAD);
    }
  }
  return original_len - len;
}

size_t SpdyFramer::ProcessCredentialFramePayload(const char* data, size_t len) {
  if (len > 0) {
    // Process only up to the end of this CREDENTIAL frame.
    len = std::min(len, remaining_control_payload_);
    bool processed_succesfully = visitor_->OnCredentialFrameData(data, len);
    remaining_control_payload_ -= len;
    remaining_data_ -= len;
    if (!processed_succesfully) {
      set_error(SPDY_CREDENTIAL_FRAME_CORRUPT);
    } else if (remaining_control_payload_ == 0) {
      visitor_->OnCredentialFrameData(NULL, 0);
      CHANGE_STATE(SPDY_AUTO_RESET);
    }
  }
  return len;
}

size_t SpdyFramer::ProcessDataFramePayload(const char* data, size_t len) {
  size_t original_len = len;

  SpdyDataFrame current_data_frame(current_frame_buffer_.get(), false);
  if (remaining_data_ > 0) {
    size_t amount_to_forward = std::min(remaining_data_, len);
    if (amount_to_forward && state_ != SPDY_IGNORE_REMAINING_PAYLOAD) {
      // Only inform the visitor if there is data.
      if (amount_to_forward) {
        visitor_->OnStreamFrameData(current_data_frame.stream_id(),
                                    data, amount_to_forward);
      }
    }
    data += amount_to_forward;
    len -= amount_to_forward;
    remaining_data_ -= amount_to_forward;

    // If the FIN flag is set, and there is no more data in this data
    // frame, inform the visitor of EOF via a 0-length data frame.
    if (!remaining_data_ &&
        current_data_frame.flags() & DATA_FLAG_FIN) {
      visitor_->OnStreamFrameData(current_data_frame.stream_id(), NULL, 0);
    }
  }

  if (remaining_data_ == 0) {
    CHANGE_STATE(SPDY_AUTO_RESET);
  }
  return original_len - len;
}

bool SpdyFramer::ParseHeaderBlockInBuffer(const char* header_data,
                                          size_t header_length,
                                          SpdyHeaderBlock* block) const {
  SpdyFrameReader reader(header_data, header_length);

  // Read number of headers.
  uint32 num_headers;
  if (spdy_version_ < 3) {
    uint16 temp;
    if (!reader.ReadUInt16(&temp)) {
      DLOG(INFO) << "Unable to read number of headers.";
      return false;
    }
    num_headers = temp;
  } else {
    if (!reader.ReadUInt32(&num_headers)) {
      DLOG(INFO) << "Unable to read number of headers.";
      return false;
    }
  }

  // Read each header.
  for (uint32 index = 0; index < num_headers; ++index) {
    base::StringPiece temp;

    // Read header name.
    if ((spdy_version_ < 3) ? !reader.ReadStringPiece16(&temp)
                            : !reader.ReadStringPiece32(&temp)) {
      DLOG(INFO) << "Unable to read header name (" << index + 1 << " of "
                 << num_headers << ").";
      return false;
    }
    std::string name = temp.as_string();

    // Read header value.
    if ((spdy_version_ < 3) ? !reader.ReadStringPiece16(&temp)
                            : !reader.ReadStringPiece32(&temp)) {
      DLOG(INFO) << "Unable to read header value (" << index + 1 << " of "
                 << num_headers << ").";
      return false;
    }
    std::string value = temp.as_string();

    // Ensure no duplicates.
    if (block->find(name) != block->end()) {
      DLOG(INFO) << "Duplicate header '" << name << "' (" << index + 1 << " of "
                 << num_headers << ").";
      return false;
    }

    // Store header.
    (*block)[name] = value;
  }
  return true;
}

// TODO(hkhalil): Remove, or move to test utils kit.
/* static */
bool SpdyFramer::ParseSettings(const SpdySettingsControlFrame* frame,
                               SettingsMap* settings) {
  DCHECK_EQ(frame->type(), SETTINGS);
  DCHECK(settings);

  SpdyFrameReader parser(frame->header_block(), frame->header_block_len());
  for (size_t index = 0; index < frame->num_entries(); ++index) {
    uint32 id_and_flags_wire;
    uint32 value;
    // SettingsFlagsAndId accepts off-the-wire (network byte order) data, so we
    // use ReadBytes() instead of ReadUInt32() as the latter calls ntohl().
    if (!parser.ReadBytes(&id_and_flags_wire, 4)) {
      return false;
    }
    if (!parser.ReadUInt32(&value))
      return false;
    SettingsFlagsAndId flags_and_id =
        SettingsFlagsAndId::FromWireFormat(frame->version(), id_and_flags_wire);
    SpdySettingsIds id = static_cast<SpdySettingsIds>(flags_and_id.id());
    SpdySettingsFlags flags =
        static_cast<SpdySettingsFlags>(flags_and_id.flags());
    (*settings)[id] = SettingsFlagsAndValue(flags, value);
  }
  return true;
}

/* static */
bool SpdyFramer::ParseCredentialData(const char* data, size_t len,
                                     SpdyCredential* credential) {
  DCHECK(credential);

  SpdyFrameReader parser(data, len);
  base::StringPiece temp;
  if (!parser.ReadUInt16(&credential->slot)) {
    return false;
  }

  if (!parser.ReadStringPiece32(&temp)) {
    return false;
  }
  credential->proof = temp.as_string();

  while (!parser.IsDoneReading()) {
    if (!parser.ReadStringPiece32(&temp)) {
      return false;
    }
    credential->certs.push_back(temp.as_string());
  }
  return true;
}

SpdySynStreamControlFrame* SpdyFramer::CreateSynStream(
    SpdyStreamId stream_id,
    SpdyStreamId associated_stream_id,
    SpdyPriority priority,
    uint8 credential_slot,
    SpdyControlFlags flags,
    bool compressed,
    const SpdyHeaderBlock* headers) {
  DCHECK_EQ(0u, stream_id & ~kStreamIdMask);
  DCHECK_EQ(0u, associated_stream_id & ~kStreamIdMask);

  // Find our length.
  size_t frame_size = SpdySynStreamControlFrame::size() +
                      GetSerializedLength(headers);

  SpdyFrameBuilder frame(SYN_STREAM, flags, spdy_version_, frame_size);
  frame.WriteUInt32(stream_id);
  frame.WriteUInt32(associated_stream_id);
  // Cap as appropriate.
  if (priority > GetLowestPriority()) {
    DLOG(DFATAL) << "Priority out-of-bounds.";
    priority = GetLowestPriority();
  }
  // Priority is 2 bits for <spdy3, 3 bits otherwise.
  frame.WriteUInt8(priority << ((spdy_version_ < 3) ? 6 : 5));
  frame.WriteUInt8((spdy_version_ < 3) ? 0 : credential_slot);
  WriteHeaderBlock(&frame, headers);
  DCHECK_EQ(frame.length(), frame_size);

  scoped_ptr<SpdySynStreamControlFrame> syn_frame(
      reinterpret_cast<SpdySynStreamControlFrame*>(frame.take()));
  if (compressed) {
    return reinterpret_cast<SpdySynStreamControlFrame*>(
        CompressControlFrame(*syn_frame.get()));
  }
  return syn_frame.release();
}

SpdySynReplyControlFrame* SpdyFramer::CreateSynReply(
    SpdyStreamId stream_id,
    SpdyControlFlags flags,
    bool compressed,
    const SpdyHeaderBlock* headers) {
  DCHECK_GT(stream_id, 0u);
  DCHECK_EQ(0u, stream_id & ~kStreamIdMask);

  // Find our length.
  size_t frame_size = SpdySynReplyControlFrame::size() +
                      GetSerializedLength(headers);
  // In SPDY 2, there were 2 unused bytes before payload.
  if (spdy_version_ < 3) {
    frame_size += 2;
  }

  SpdyFrameBuilder frame(SYN_REPLY, flags, spdy_version_, frame_size);
  frame.WriteUInt32(stream_id);
  if (spdy_version_ < 3) {
    frame.WriteUInt16(0);  // Unused
  }
  WriteHeaderBlock(&frame, headers);
  DCHECK_EQ(frame.length(), frame_size);

  scoped_ptr<SpdySynReplyControlFrame> reply_frame(
      reinterpret_cast<SpdySynReplyControlFrame*>(frame.take()));
  if (compressed) {
    return reinterpret_cast<SpdySynReplyControlFrame*>(
        CompressControlFrame(*reply_frame.get()));
  }
  return reply_frame.release();
}

SpdyRstStreamControlFrame* SpdyFramer::CreateRstStream(
    SpdyStreamId stream_id,
    SpdyStatusCodes status) const {
  DCHECK_GT(stream_id, 0u);
  DCHECK_EQ(0u, stream_id & ~kStreamIdMask);
  DCHECK_NE(status, INVALID);
  DCHECK_LT(status, NUM_STATUS_CODES);

  size_t frame_size = SpdyRstStreamControlFrame::size();
  SpdyFrameBuilder frame(RST_STREAM, CONTROL_FLAG_NONE, spdy_version_,
                         frame_size);
  frame.WriteUInt32(stream_id);
  frame.WriteUInt32(status);
  DCHECK_EQ(frame.length(), frame_size);
  return reinterpret_cast<SpdyRstStreamControlFrame*>(frame.take());
}

SpdySettingsControlFrame* SpdyFramer::CreateSettings(
    const SettingsMap& values) const {
  size_t frame_size = SpdySettingsControlFrame::size() + 8 * values.size();
  SpdyFrameBuilder frame(SETTINGS, CONTROL_FLAG_NONE, spdy_version_,
                         frame_size);
  frame.WriteUInt32(values.size());
  for (SettingsMap::const_iterator it = values.begin();
       it != values.end();
       it++) {
    SettingsFlagsAndId flags_and_id(it->second.first, it->first);
    uint32 id_and_flags_wire = flags_and_id.GetWireFormat(spdy_version_);
    frame.WriteBytes(&id_and_flags_wire, 4);
    frame.WriteUInt32(it->second.second);
  }
  DCHECK_EQ(frame.length(), frame_size);
  return reinterpret_cast<SpdySettingsControlFrame*>(frame.take());
}

SpdyPingControlFrame* SpdyFramer::CreatePingFrame(uint32 unique_id) const {
  size_t frame_size = SpdyPingControlFrame::size();
  SpdyFrameBuilder frame(PING, CONTROL_FLAG_NONE, spdy_version_, frame_size);
  frame.WriteUInt32(unique_id);
  DCHECK_EQ(frame.length(), frame_size);
  return reinterpret_cast<SpdyPingControlFrame*>(frame.take());
}

SpdyGoAwayControlFrame* SpdyFramer::CreateGoAway(
    SpdyStreamId last_accepted_stream_id,
    SpdyGoAwayStatus status) const {
  DCHECK_EQ(0u, last_accepted_stream_id & ~kStreamIdMask);

  // SPDY 2 GOAWAY frames are 4 bytes smaller than in SPDY 3. We account for
  // this difference via a separate offset variable, since
  // SpdyGoAwayControlFrame::size() returns the SPDY 3 size.
  const size_t goaway_offset = (protocol_version() < 3) ? 4 : 0;
  size_t frame_size = SpdyGoAwayControlFrame::size() - goaway_offset;
  SpdyFrameBuilder frame(GOAWAY, CONTROL_FLAG_NONE, spdy_version_, frame_size);
  frame.WriteUInt32(last_accepted_stream_id);
  if (protocol_version() >= 3) {
    frame.WriteUInt32(status);
  }
  DCHECK_EQ(frame.length(), frame_size);
  return reinterpret_cast<SpdyGoAwayControlFrame*>(frame.take());
}

SpdyHeadersControlFrame* SpdyFramer::CreateHeaders(
    SpdyStreamId stream_id,
    SpdyControlFlags flags,
    bool compressed,
    const SpdyHeaderBlock* headers) {
  // Basically the same as CreateSynReply().
  DCHECK_GT(stream_id, 0u);
  DCHECK_EQ(0u, stream_id & ~kStreamIdMask);

  // Find our length.
  size_t frame_size = SpdyHeadersControlFrame::size() +
                      GetSerializedLength(headers);
  // In SPDY 2, there were 2 unused bytes before payload.
  if (spdy_version_ < 3) {
    frame_size += 2;
  }

  SpdyFrameBuilder frame(HEADERS, flags, spdy_version_, frame_size);
  frame.WriteUInt32(stream_id);
  if (spdy_version_ < 3) {
    frame.WriteUInt16(0);  // Unused
  }
  WriteHeaderBlock(&frame, headers);
  DCHECK_EQ(frame.length(), frame_size);

  scoped_ptr<SpdyHeadersControlFrame> headers_frame(
      reinterpret_cast<SpdyHeadersControlFrame*>(frame.take()));
  if (compressed) {
    return reinterpret_cast<SpdyHeadersControlFrame*>(
        CompressControlFrame(*headers_frame.get()));
  }
  return headers_frame.release();
}

SpdyWindowUpdateControlFrame* SpdyFramer::CreateWindowUpdate(
    SpdyStreamId stream_id,
    uint32 delta_window_size) const {
  DCHECK_GT(stream_id, 0u);
  DCHECK_EQ(0u, stream_id & ~kStreamIdMask);
  DCHECK_GT(delta_window_size, 0u);
  DCHECK_LE(delta_window_size,
            static_cast<uint32>(kSpdyStreamMaximumWindowSize));

  size_t frame_size = SpdyWindowUpdateControlFrame::size();
  SpdyFrameBuilder frame(WINDOW_UPDATE, CONTROL_FLAG_NONE, spdy_version_,
                         frame_size);
  frame.WriteUInt32(stream_id);
  frame.WriteUInt32(delta_window_size);
  DCHECK_EQ(frame.length(), frame_size);
  return reinterpret_cast<SpdyWindowUpdateControlFrame*>(frame.take());
}

SpdyCredentialControlFrame* SpdyFramer::CreateCredentialFrame(
    const SpdyCredential& credential) const {
  // Calculate the size of the frame by adding the size of the
  // variable length data to the size of the fixed length data.
  size_t frame_size =  SpdyCredentialControlFrame::size() +
      credential.proof.length();
  DCHECK_EQ(SpdyCredentialControlFrame::size(), 14u);
  for (std::vector<std::string>::const_iterator cert = credential.certs.begin();
       cert != credential.certs.end();
       ++cert) {
    frame_size += sizeof(uint32);  // size of the cert_length field
    frame_size += cert->length();     // size of the cert_data field
  }

  SpdyFrameBuilder frame(CREDENTIAL, CONTROL_FLAG_NONE, spdy_version_,
                         frame_size);
  frame.WriteUInt16(credential.slot);
  frame.WriteUInt32(credential.proof.size());
  frame.WriteBytes(credential.proof.c_str(), credential.proof.size());
  for (std::vector<std::string>::const_iterator cert = credential.certs.begin();
       cert != credential.certs.end();
       ++cert) {
    frame.WriteUInt32(cert->length());
    frame.WriteBytes(cert->c_str(), cert->length());
  }
  DCHECK_EQ(frame.length(), frame_size);
  return reinterpret_cast<SpdyCredentialControlFrame*>(frame.take());
}

SpdyDataFrame* SpdyFramer::CreateDataFrame(
    SpdyStreamId stream_id,
                                           const char* data,
    uint32 len, SpdyDataFlags flags) const {
  DCHECK_EQ(0u, stream_id & ~kStreamIdMask);
  size_t frame_size = SpdyDataFrame::size() + len;
  SpdyFrameBuilder frame(stream_id, flags, frame_size);
  frame.WriteBytes(data, len);
  DCHECK_EQ(frame.length(), frame_size);
  return reinterpret_cast<SpdyDataFrame*>(frame.take());
}

// The following compression setting are based on Brian Olson's analysis. See
// https://groups.google.com/group/spdy-dev/browse_thread/thread/dfaf498542fac792
// for more details.
static const int kCompressorLevel = 9;
static const int kCompressorWindowSizeInBits = 11;
static const int kCompressorMemLevel = 1;

z_stream* SpdyFramer::GetHeaderCompressor() {
  if (header_compressor_.get())
    return header_compressor_.get();  // Already initialized.

  header_compressor_.reset(new z_stream);
  memset(header_compressor_.get(), 0, sizeof(z_stream));

  int success = deflateInit2(header_compressor_.get(),
                             kCompressorLevel,
                             Z_DEFLATED,
                             kCompressorWindowSizeInBits,
                             kCompressorMemLevel,
                             Z_DEFAULT_STRATEGY);
  if (success == Z_OK) {
    const char* dictionary = (spdy_version_ < 3) ? kV2Dictionary
                                                 : kV3Dictionary;
    const int dictionary_size = (spdy_version_ < 3) ? kV2DictionarySize
                                                    : kV3DictionarySize;
    success = deflateSetDictionary(header_compressor_.get(),
                                   reinterpret_cast<const Bytef*>(dictionary),
                                   dictionary_size);
  }
  if (success != Z_OK) {
    LOG(WARNING) << "deflateSetDictionary failure: " << success;
    header_compressor_.reset(NULL);
    return NULL;
  }
  return header_compressor_.get();
}

z_stream* SpdyFramer::GetHeaderDecompressor() {
  if (header_decompressor_.get())
    return header_decompressor_.get();  // Already initialized.

  header_decompressor_.reset(new z_stream);
  memset(header_decompressor_.get(), 0, sizeof(z_stream));

  int success = inflateInit(header_decompressor_.get());
  if (success != Z_OK) {
    LOG(WARNING) << "inflateInit failure: " << success;
    header_decompressor_.reset(NULL);
    return NULL;
  }
  return header_decompressor_.get();
}

bool SpdyFramer::GetFrameBoundaries(const SpdyFrame& frame,
                                    int* payload_length,
                                    int* header_length,
                                    const char** payload) const {
  size_t frame_size;
  if (frame.is_control_frame()) {
    const SpdyControlFrame& control_frame =
        reinterpret_cast<const SpdyControlFrame&>(frame);
    switch (control_frame.type()) {
      case SYN_STREAM:
        {
          const SpdySynStreamControlFrame& syn_frame =
              reinterpret_cast<const SpdySynStreamControlFrame&>(frame);
          frame_size = SpdySynStreamControlFrame::size();
          *payload_length = syn_frame.header_block_len();
          *header_length = frame_size;
          *payload = frame.data() + *header_length;
        }
        break;
      case SYN_REPLY:
        {
          const SpdySynReplyControlFrame& syn_frame =
              reinterpret_cast<const SpdySynReplyControlFrame&>(frame);
          frame_size = SpdySynReplyControlFrame::size();
          *payload_length = syn_frame.header_block_len();
          *header_length = frame_size;
          *payload = frame.data() + *header_length;
          // SPDY 2 had two bytes of unused space preceeding payload.
          if (spdy_version_ < 3) {
            *header_length += 2;
            *payload += 2;
          }
        }
        break;
      case HEADERS:
        {
          const SpdyHeadersControlFrame& headers_frame =
              reinterpret_cast<const SpdyHeadersControlFrame&>(frame);
          frame_size = SpdyHeadersControlFrame::size();
          *payload_length = headers_frame.header_block_len();
          *header_length = frame_size;
          *payload = frame.data() + *header_length;
          // SPDY 2 had two bytes of unused space preceeding payload.
          if (spdy_version_ < 3) {
            *header_length += 2;
            *payload += 2;
          }
        }
        break;
      default:
        // TODO(mbelshe): set an error?
        return false;  // We can't compress this frame!
    }
  } else {
    frame_size = SpdyFrame::kHeaderSize;
    *header_length = frame_size;
    *payload_length = frame.length();
    *payload = frame.data() + SpdyFrame::kHeaderSize;
  }
  return true;
}

SpdyControlFrame* SpdyFramer::CompressControlFrame(
    const SpdyControlFrame& frame) {
  z_stream* compressor = GetHeaderCompressor();
  if (!compressor)
    return NULL;

  int payload_length;
  int header_length;
  const char* payload;

  base::StatsCounter compressed_frames("spdy.CompressedFrames");
  base::StatsCounter pre_compress_bytes("spdy.PreCompressSize");
  base::StatsCounter post_compress_bytes("spdy.PostCompressSize");

  if (!enable_compression_)
    return reinterpret_cast<SpdyControlFrame*>(DuplicateFrame(frame));

  if (!GetFrameBoundaries(frame, &payload_length, &header_length, &payload))
    return NULL;

  // Create an output frame.
  int compressed_max_size = deflateBound(compressor, payload_length);
  size_t new_frame_size = header_length + compressed_max_size;
  if ((frame.type() == SYN_REPLY || frame.type() == HEADERS) &&
      spdy_version_ < 3) {
    new_frame_size += 2;
  }
  DCHECK_GE(new_frame_size, frame.length() + SpdyFrame::kHeaderSize);
  scoped_ptr<SpdyControlFrame> new_frame(new SpdyControlFrame(new_frame_size));
  memcpy(new_frame->data(), frame.data(),
         frame.length() + SpdyFrame::kHeaderSize);

  compressor->next_in = reinterpret_cast<Bytef*>(const_cast<char*>(payload));
  compressor->avail_in = payload_length;
  compressor->next_out = reinterpret_cast<Bytef*>(new_frame->data()) +
                          header_length;
  compressor->avail_out = compressed_max_size;

  // Make sure that all the data we pass to zlib is defined.
  // This way, all Valgrind reports on the compressed data are zlib's fault.
  (void)VALGRIND_CHECK_MEM_IS_DEFINED(compressor->next_in,
                                      compressor->avail_in);

  int rv = deflate(compressor, Z_SYNC_FLUSH);
  if (rv != Z_OK) {  // How can we know that it compressed everything?
    // This shouldn't happen, right?
    LOG(WARNING) << "deflate failure: " << rv;
    return NULL;
  }

  int compressed_size = compressed_max_size - compressor->avail_out;

  // We trust zlib. Also, we can't do anything about it.
  // See http://www.zlib.net/zlib_faq.html#faq36
  (void)VALGRIND_MAKE_MEM_DEFINED(new_frame->data() + header_length,
                                  compressed_size);

  new_frame->set_length(
      header_length + compressed_size - SpdyFrame::kHeaderSize);

  pre_compress_bytes.Add(payload_length);
  post_compress_bytes.Add(new_frame->length());

  compressed_frames.Increment();

  return new_frame.release();
}

// Incrementally decompress the control frame's header block, feeding the
// result to the visitor in chunks. Continue this until the visitor
// indicates that it cannot process any more data, or (more commonly) we
// run out of data to deliver.
bool SpdyFramer::IncrementallyDecompressControlFrameHeaderData(
    const SpdyControlFrame* control_frame,
    const char* data,
    size_t len) {
  // Get a decompressor or set error.
  z_stream* decomp = GetHeaderDecompressor();
  if (decomp == NULL) {
    LOG(DFATAL) << "Couldn't get decompressor for handling compressed headers.";
    set_error(SPDY_DECOMPRESS_FAILURE);
    return false;
  }

  bool processed_successfully = true;
  char buffer[kHeaderDataChunkMaxSize];

  decomp->next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data));
  decomp->avail_in = len;
  const SpdyStreamId stream_id = GetControlFrameStreamId(control_frame);
  DCHECK_LT(0u, stream_id);
  while (decomp->avail_in > 0 && processed_successfully) {
    decomp->next_out = reinterpret_cast<Bytef*>(buffer);
    decomp->avail_out = arraysize(buffer);

    int rv = inflate(decomp, Z_SYNC_FLUSH);
    if (rv == Z_NEED_DICT) {
      const char* dictionary = (spdy_version_ < 3) ? kV2Dictionary
                                                   : kV3Dictionary;
      const int dictionary_size = (spdy_version_ < 3) ? kV2DictionarySize
                                                      : kV3DictionarySize;
      const DictionaryIds& ids = g_dictionary_ids.Get();
      const uLong dictionary_id = (spdy_version_ < 3) ? ids.v2_dictionary_id
                                                      : ids.v3_dictionary_id;
      // Need to try again with the right dictionary.
      if (decomp->adler == dictionary_id) {
        rv = inflateSetDictionary(decomp,
                                  reinterpret_cast<const Bytef*>(dictionary),
                                  dictionary_size);
        if (rv == Z_OK)
          rv = inflate(decomp, Z_SYNC_FLUSH);
      }
    }

    // Inflate will generate a Z_BUF_ERROR if it runs out of input
    // without producing any output.  The input is consumed and
    // buffered internally by zlib so we can detect this condition by
    // checking if avail_in is 0 after the call to inflate.
    bool input_exhausted = ((rv == Z_BUF_ERROR) && (decomp->avail_in == 0));
    if ((rv == Z_OK) || input_exhausted) {
      size_t decompressed_len = arraysize(buffer) - decomp->avail_out;
      if (decompressed_len > 0) {
        processed_successfully = visitor_->OnControlFrameHeaderData(
            stream_id, buffer, decompressed_len);
      }
      if (!processed_successfully) {
        // Assume that the problem was the header block was too large for the
        // visitor.
        set_error(SPDY_CONTROL_PAYLOAD_TOO_LARGE);
      }
    } else {
      DLOG(WARNING) << "inflate failure: " << rv << " " << len;
      set_error(SPDY_DECOMPRESS_FAILURE);
      processed_successfully = false;
    }
  }
  return processed_successfully;
}

bool SpdyFramer::IncrementallyDeliverControlFrameHeaderData(
    const SpdyControlFrame* control_frame, const char* data, size_t len) {
  bool read_successfully = true;
  const SpdyStreamId stream_id = GetControlFrameStreamId(control_frame);
  while (read_successfully && len > 0) {
    size_t bytes_to_deliver = std::min(len, kHeaderDataChunkMaxSize);
    read_successfully = visitor_->OnControlFrameHeaderData(stream_id, data,
                                                           bytes_to_deliver);
    data += bytes_to_deliver;
    len -= bytes_to_deliver;
    if (!read_successfully) {
      // Assume that the problem was the header block was too large for the
      // visitor.
      set_error(SPDY_CONTROL_PAYLOAD_TOO_LARGE);
    }
  }
  return read_successfully;
}

SpdyFrame* SpdyFramer::DuplicateFrame(const SpdyFrame& frame) {
  int size = SpdyFrame::kHeaderSize + frame.length();
  SpdyFrame* new_frame = new SpdyFrame(size);
  memcpy(new_frame->data(), frame.data(), size);
  return new_frame;
}

bool SpdyFramer::IsCompressible(const SpdyFrame& frame) const {
  // The important frames to compress are those which contain large
  // amounts of compressible data - namely the headers in the SYN_STREAM
  // and SYN_REPLY.
  if (frame.is_control_frame()) {
    const SpdyControlFrame& control_frame =
        reinterpret_cast<const SpdyControlFrame&>(frame);
    return control_frame.type() == SYN_STREAM ||
        control_frame.type() == SYN_REPLY ||
        control_frame.type() == HEADERS;
  }

  // We don't compress Data frames.
  return false;
}

size_t SpdyFramer::GetMinimumControlFrameSize(int version,
                                              SpdyControlType type) {
  switch (type) {
    case SYN_STREAM:
      return SpdySynStreamControlFrame::size();
    case SYN_REPLY:
      return SpdySynReplyControlFrame::size();
    case RST_STREAM:
      return SpdyRstStreamControlFrame::size();
    case SETTINGS:
      return SpdySettingsControlFrame::size();
    case NOOP:
      // Even though NOOP is no longer supported, we still correctly report its
      // size so that it can be handled correctly as incoming data if
      // implementations so desire.
      return SpdyFrame::kHeaderSize;
    case PING:
      return SpdyPingControlFrame::size();
    case GOAWAY:
      if (version < 3) {
        // SPDY 2 GOAWAY is smaller by 32 bits. Since
        // SpdyGoAwayControlFrame::size() returns the size for SPDY 3, we adjust
        // before returning here.
        return SpdyGoAwayControlFrame::size() - 4;
      } else {
        return SpdyGoAwayControlFrame::size();
      }
    case HEADERS:
      return SpdyHeadersControlFrame::size();
    case WINDOW_UPDATE:
      return SpdyWindowUpdateControlFrame::size();
    case CREDENTIAL:
      return SpdyCredentialControlFrame::size();
    case NUM_CONTROL_FRAME_TYPES:
      break;
  }
  LOG(ERROR) << "Unknown control frame type " << type;
  return std::numeric_limits<size_t>::max();
}

/* static */
SpdyStreamId SpdyFramer::GetControlFrameStreamId(
    const SpdyControlFrame* control_frame) {
  SpdyStreamId stream_id = kInvalidStream;
  if (control_frame != NULL) {
    switch (control_frame->type()) {
      case SYN_STREAM:
        stream_id = reinterpret_cast<const SpdySynStreamControlFrame*>(
            control_frame)->stream_id();
        break;
      case SYN_REPLY:
        stream_id = reinterpret_cast<const SpdySynReplyControlFrame*>(
            control_frame)->stream_id();
        break;
      case HEADERS:
        stream_id = reinterpret_cast<const SpdyHeadersControlFrame*>(
            control_frame)->stream_id();
        break;
      case RST_STREAM:
        stream_id = reinterpret_cast<const SpdyRstStreamControlFrame*>(
            control_frame)->stream_id();
        break;
      case WINDOW_UPDATE:
        stream_id = reinterpret_cast<const SpdyWindowUpdateControlFrame*>(
            control_frame)->stream_id();
        break;
      // All of the following types are not part of a particular stream.
      // They all fall through to the invalid control frame type case.
      // (The default case isn't used so that the compile will break if a new
      // control frame type is added but not included here.)
      case SETTINGS:
      case NOOP:
      case PING:
      case GOAWAY:
      case CREDENTIAL:
      case NUM_CONTROL_FRAME_TYPES:  // makes compiler happy
        break;
    }
  }
  return stream_id;
}

void SpdyFramer::set_enable_compression(bool value) {
  enable_compression_ = value;
}

}  // namespace net
