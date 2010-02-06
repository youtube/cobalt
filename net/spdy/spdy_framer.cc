// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "base/stats_counters.h"

#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_bitmasks.h"

#if defined(USE_SYSTEM_ZLIB)
#include <zlib.h>
#else
#include "third_party/zlib/zlib.h"
#endif

namespace flip {

// The initial size of the control frame buffer; this is used internally
// as we parse through control frames.
static const size_t kControlFrameBufferInitialSize = 32 * 1024;
// The maximum size of the control frame buffer that we support.
// TODO(mbelshe): We should make this stream-based so there are no limits.
static const size_t kControlFrameBufferMaxSize = 64 * 1024;

// By default is compression on or off.
bool FlipFramer::compression_default_ = true;

#ifdef DEBUG_FLIP_STATE_CHANGES
#define CHANGE_STATE(newstate) \
{ \
  do { \
    LOG(INFO) << "Changing state from: " \
      << StateToString(state_) \
      << " to " << StateToString(newstate) << "\n"; \
    state_ = newstate; \
  } while (false); \
}
#else
#define CHANGE_STATE(newstate) (state_ = newstate)
#endif

FlipFramer::FlipFramer()
    : state_(FLIP_RESET),
      error_code_(FLIP_NO_ERROR),
      remaining_payload_(0),
      remaining_control_payload_(0),
      current_frame_buffer_(NULL),
      current_frame_len_(0),
      current_frame_capacity_(0),
      enable_compression_(compression_default_),
      visitor_(NULL) {
}

FlipFramer::~FlipFramer() {
  if (compressor_.get()) {
    deflateEnd(compressor_.get());
  }
  if (decompressor_.get()) {
    inflateEnd(decompressor_.get());
  }
  delete [] current_frame_buffer_;
}

void FlipFramer::Reset() {
  state_ = FLIP_RESET;
  error_code_ = FLIP_NO_ERROR;
  remaining_payload_ = 0;
  remaining_control_payload_ = 0;
  current_frame_len_ = 0;
  if (current_frame_capacity_ != kControlFrameBufferInitialSize) {
    delete [] current_frame_buffer_;
    current_frame_buffer_ = 0;
    current_frame_capacity_ = 0;
    ExpandControlFrameBuffer(kControlFrameBufferInitialSize);
  }
}

const char* FlipFramer::StateToString(int state) {
  switch (state) {
    case FLIP_ERROR:
      return "ERROR";
    case FLIP_DONE:
      return "DONE";
    case FLIP_AUTO_RESET:
      return "AUTO_RESET";
    case FLIP_RESET:
      return "RESET";
    case FLIP_READING_COMMON_HEADER:
      return "READING_COMMON_HEADER";
    case FLIP_INTERPRET_CONTROL_FRAME_COMMON_HEADER:
      return "INTERPRET_CONTROL_FRAME_COMMON_HEADER";
    case FLIP_CONTROL_FRAME_PAYLOAD:
      return "CONTROL_FRAME_PAYLOAD";
    case FLIP_IGNORE_REMAINING_PAYLOAD:
      return "IGNORE_REMAINING_PAYLOAD";
    case FLIP_FORWARD_STREAM_FRAME:
      return "FORWARD_STREAM_FRAME";
  }
  return "UNKNOWN_STATE";
}

size_t FlipFramer::BytesSafeToRead() const {
  switch (state_) {
    case FLIP_ERROR:
    case FLIP_DONE:
    case FLIP_AUTO_RESET:
    case FLIP_RESET:
      return 0;
    case FLIP_READING_COMMON_HEADER:
      DCHECK(current_frame_len_ < FlipFrame::size());
      return FlipFrame::size() - current_frame_len_;
    case FLIP_INTERPRET_CONTROL_FRAME_COMMON_HEADER:
      return 0;
    case FLIP_CONTROL_FRAME_PAYLOAD:
    case FLIP_IGNORE_REMAINING_PAYLOAD:
    case FLIP_FORWARD_STREAM_FRAME:
      return remaining_payload_;
  }
  // We should never get to here.
  return 0;
}

void FlipFramer::set_error(FlipError error) {
  DCHECK(visitor_);
  error_code_ = error;
  CHANGE_STATE(FLIP_ERROR);
  visitor_->OnError(this);
}

const char* FlipFramer::ErrorCodeToString(int error_code) {
  switch (error_code) {
    case FLIP_NO_ERROR:
      return "NO_ERROR";
    case FLIP_UNKNOWN_CONTROL_TYPE:
      return "UNKNOWN_CONTROL_TYPE";
    case FLIP_INVALID_CONTROL_FRAME:
      return "INVALID_CONTROL_FRAME";
    case FLIP_CONTROL_PAYLOAD_TOO_LARGE:
      return "CONTROL_PAYLOAD_TOO_LARGE";
    case FLIP_ZLIB_INIT_FAILURE:
      return "ZLIB_INIT_FAILURE";
    case FLIP_UNSUPPORTED_VERSION:
      return "UNSUPPORTED_VERSION";
    case FLIP_DECOMPRESS_FAILURE:
      return "DECOMPRESS_FAILURE";
  }
  return "UNKNOWN_STATE";
}

size_t FlipFramer::ProcessInput(const char* data, size_t len) {
  DCHECK(visitor_);
  DCHECK(data);

  size_t original_len = len;
  while (len != 0) {
    switch (state_) {
      case FLIP_ERROR:
      case FLIP_DONE:
        goto bottom;

      case FLIP_AUTO_RESET:
      case FLIP_RESET:
        Reset();
        CHANGE_STATE(FLIP_READING_COMMON_HEADER);
        continue;

      case FLIP_READING_COMMON_HEADER: {
        int bytes_read = ProcessCommonHeader(data, len);
        len -= bytes_read;
        data += bytes_read;
        continue;
      }

      // Arguably, this case is not necessary, as no bytes are consumed here.
      // I felt it was a nice partitioning, however (which probably indicates
      // that it should be refactored into its own function!)
      case FLIP_INTERPRET_CONTROL_FRAME_COMMON_HEADER:
        ProcessControlFrameHeader();
        continue;

      case FLIP_CONTROL_FRAME_PAYLOAD: {
        int bytes_read = ProcessControlFramePayload(data, len);
        len -= bytes_read;
        data += bytes_read;
      }
        // intentional fallthrough
      case FLIP_IGNORE_REMAINING_PAYLOAD:
        // control frame has too-large payload
        // intentional fallthrough
      case FLIP_FORWARD_STREAM_FRAME: {
        int bytes_read = ProcessDataFramePayload(data, len);
        len -= bytes_read;
        data += bytes_read;
        continue;
      }
      default:
        break;
    }
  }
 bottom:
  return original_len - len;
}

size_t FlipFramer::ProcessCommonHeader(const char* data, size_t len) {
  // This should only be called when we're in the FLIP_READING_COMMON_HEADER
  // state.
  DCHECK(state_ == FLIP_READING_COMMON_HEADER);

  int original_len = len;
  FlipFrame current_frame(current_frame_buffer_, false);

  do {
    if (current_frame_len_ < FlipFrame::size()) {
      size_t bytes_desired = FlipFrame::size() - current_frame_len_;
      size_t bytes_to_append = std::min(bytes_desired, len);
      char* header_buffer = current_frame_buffer_;
      memcpy(&header_buffer[current_frame_len_], data, bytes_to_append);
      current_frame_len_ += bytes_to_append;
      data += bytes_to_append;
      len -= bytes_to_append;
      // Check for an empty data frame.
      if (current_frame_len_ == FlipFrame::size() &&
          !current_frame.is_control_frame() &&
          current_frame.length() == 0) {
        if (current_frame.flags() & CONTROL_FLAG_FIN) {
          FlipDataFrame data_frame(current_frame_buffer_, false);
          visitor_->OnStreamFrameData(data_frame.stream_id(), NULL, 0);
        }
        CHANGE_STATE(FLIP_RESET);
      }
      break;
    }
    remaining_payload_ = current_frame.length();

    // This is just a sanity check for help debugging early frame errors.
    if (remaining_payload_ > 1000000u) {
      LOG(ERROR) <<
          "Unexpectedly large frame.  Flip session is likely corrupt.";
    }

    // if we're here, then we have the common header all received.
    if (!current_frame.is_control_frame())
      CHANGE_STATE(FLIP_FORWARD_STREAM_FRAME);
    else
      CHANGE_STATE(FLIP_INTERPRET_CONTROL_FRAME_COMMON_HEADER);
  } while (false);

  return original_len - len;
}

void FlipFramer::ProcessControlFrameHeader() {
  DCHECK_EQ(FLIP_NO_ERROR, error_code_);
  DCHECK_LE(FlipFrame::size(), current_frame_len_);
  FlipControlFrame current_control_frame(current_frame_buffer_, false);
  // Do some sanity checking on the control frame sizes.
  switch (current_control_frame.type()) {
    case SYN_STREAM:
      if (current_control_frame.length() <
          FlipSynStreamControlFrame::size() - FlipControlFrame::size())
        set_error(FLIP_INVALID_CONTROL_FRAME);
      break;
    case SYN_REPLY:
      if (current_control_frame.length() <
          FlipSynReplyControlFrame::size() - FlipControlFrame::size())
        set_error(FLIP_INVALID_CONTROL_FRAME);
      break;
    case FIN_STREAM:
      if (current_control_frame.length() !=
          FlipFinStreamControlFrame::size() - FlipFrame::size())
        set_error(FLIP_INVALID_CONTROL_FRAME);
      break;
    case NOOP:
      // NOOP.  Swallow it.
      CHANGE_STATE(FLIP_AUTO_RESET);
      return;
    default:
      set_error(FLIP_UNKNOWN_CONTROL_TYPE);
      break;
  }

  // We only support version 1 of this protocol.
  if (current_control_frame.version() != kFlipProtocolVersion)
    set_error(FLIP_UNSUPPORTED_VERSION);

  remaining_control_payload_ = current_control_frame.length();
  if (remaining_control_payload_ > kControlFrameBufferMaxSize)
    set_error(FLIP_CONTROL_PAYLOAD_TOO_LARGE);

  if (error_code_)
    return;

  ExpandControlFrameBuffer(remaining_control_payload_);
  CHANGE_STATE(FLIP_CONTROL_FRAME_PAYLOAD);
}

size_t FlipFramer::ProcessControlFramePayload(const char* data, size_t len) {
  size_t original_len = len;
  do {
    if (remaining_control_payload_) {
      size_t amount_to_consume = std::min(remaining_control_payload_, len);
      memcpy(&current_frame_buffer_[current_frame_len_], data,
             amount_to_consume);
      current_frame_len_ += amount_to_consume;
      data += amount_to_consume;
      len -= amount_to_consume;
      remaining_control_payload_ -= amount_to_consume;
      remaining_payload_ -= amount_to_consume;
      if (remaining_control_payload_)
        break;
    }
    FlipControlFrame control_frame(current_frame_buffer_, false);
    visitor_->OnControl(&control_frame);

    // If this is a FIN, tell the caller.
    if (control_frame.type() == SYN_REPLY &&
        control_frame.flags() & CONTROL_FLAG_FIN)
      visitor_->OnStreamFrameData(control_frame.stream_id(), NULL, 0);

    CHANGE_STATE(FLIP_IGNORE_REMAINING_PAYLOAD);
  } while (false);
  return original_len - len;
}

size_t FlipFramer::ProcessDataFramePayload(const char* data, size_t len) {
  size_t original_len = len;

  FlipDataFrame current_data_frame(current_frame_buffer_, false);
  if (remaining_payload_) {
    size_t amount_to_forward = std::min(remaining_payload_, len);
    if (amount_to_forward && state_ != FLIP_IGNORE_REMAINING_PAYLOAD) {
      if (current_data_frame.flags() & DATA_FLAG_COMPRESSED) {
        // TODO(mbelshe): Assert that the decompressor is init'ed.
        if (!InitializeDecompressor())
          return NULL;

        size_t decompressed_max_size = amount_to_forward * 100;
        scoped_ptr<char> decompressed(new char[decompressed_max_size]);
        decompressor_->next_in = reinterpret_cast<Bytef*>(
            const_cast<char*>(data));
        decompressor_->avail_in = amount_to_forward;
        decompressor_->next_out =
            reinterpret_cast<Bytef*>(decompressed.get());
        decompressor_->avail_out = decompressed_max_size;

        int rv = inflate(decompressor_.get(), Z_SYNC_FLUSH);
        if (rv != Z_OK) {
          set_error(FLIP_DECOMPRESS_FAILURE);
          return 0;
        }
        size_t decompressed_size = decompressed_max_size -
                                   decompressor_->avail_out;
        // Only inform the visitor if there is data.
        if (decompressed_size)
          visitor_->OnStreamFrameData(current_data_frame.stream_id(),
                                      decompressed.get(),
                                      decompressed_size);
        amount_to_forward -= decompressor_->avail_in;
      } else {
        // The data frame was not compressed.
        // Only inform the visitor if there is data.
        if (amount_to_forward)
          visitor_->OnStreamFrameData(current_data_frame.stream_id(),
                                      data, amount_to_forward);
      }
    }
    data += amount_to_forward;
    len -= amount_to_forward;
    remaining_payload_ -= amount_to_forward;

    // If the FIN flag is set, and there is no more data in this data
    // frame, inform the visitor of EOF via a 0-length data frame.
    if (!remaining_payload_ &&
        current_data_frame.flags() & DATA_FLAG_FIN)
      visitor_->OnStreamFrameData(current_data_frame.stream_id(), NULL,
                                  0);
  } else {
    CHANGE_STATE(FLIP_AUTO_RESET);
  }
  return original_len - len;
}

void FlipFramer::ExpandControlFrameBuffer(size_t size) {
  DCHECK(size < kControlFrameBufferMaxSize);
  if (size < current_frame_capacity_)
    return;

  int alloc_size = size + FlipFrame::size();
  char* new_buffer = new char[alloc_size];
  memcpy(new_buffer, current_frame_buffer_, current_frame_len_);
  current_frame_capacity_ = alloc_size;
  current_frame_buffer_ = new_buffer;
}

bool FlipFramer::ParseHeaderBlock(const FlipFrame* frame,
                                  FlipHeaderBlock* block) {
  FlipControlFrame control_frame(frame->data(), false);
  uint32 type = control_frame.type();
  if (type != SYN_STREAM && type != SYN_REPLY)
    return false;

  // Find the header data within the control frame.
  scoped_ptr<FlipFrame> decompressed_frame(DecompressFrame(frame));
  if (!decompressed_frame.get())
    return false;
  FlipSynStreamControlFrame syn_frame(decompressed_frame->data(), false);
  const char *header_data = syn_frame.header_block();
  int header_length = syn_frame.header_block_len();

  FlipFrameBuilder builder(header_data, header_length);
  void* iter = NULL;
  uint16 num_headers;
  if (builder.ReadUInt16(&iter, &num_headers)) {
    for (int index = 0; index < num_headers; ++index) {
      std::string name;
      std::string value;
      if (!builder.ReadString(&iter, &name))
        break;
      if (!builder.ReadString(&iter, &value))
        break;
      if (block->find(name) == block->end()) {
        (*block)[name] = value;
      } else {
        return false;
      }
    }
    return true;
  }
  return false;
}

FlipSynStreamControlFrame* FlipFramer::CreateSynStream(
    FlipStreamId stream_id, int priority, FlipControlFlags flags,
    bool compressed, FlipHeaderBlock* headers) {
  FlipFrameBuilder frame;

  frame.WriteUInt16(kControlFlagMask | kFlipProtocolVersion);
  frame.WriteUInt16(SYN_STREAM);
  frame.WriteUInt32(0);  // Placeholder for the length and flags
  frame.WriteUInt32(stream_id);
  frame.WriteUInt16(ntohs(priority) << 6);  // Priority.

  frame.WriteUInt16(headers->size());  // Number of headers.
  FlipHeaderBlock::iterator it;
  for (it = headers->begin(); it != headers->end(); ++it) {
    frame.WriteString(it->first);
    frame.WriteString(it->second);
  }

  // Write the length and flags.
  size_t length = frame.length() - FlipFrame::size();
  DCHECK(length < static_cast<size_t>(kLengthMask));
  FlagsAndLength flags_length;
  flags_length.length_ = htonl(static_cast<uint32>(length));
  flags_length.flags_[0] = flags;
  frame.WriteBytesToOffset(4, &flags_length, sizeof(flags_length));

  scoped_ptr<FlipFrame> syn_frame(frame.take());
  if (compressed) {
    return reinterpret_cast<FlipSynStreamControlFrame*>(
        CompressFrame(syn_frame.get()));
  }
  return reinterpret_cast<FlipSynStreamControlFrame*>(syn_frame.release());
}

/* static */
FlipFinStreamControlFrame* FlipFramer::CreateFinStream(FlipStreamId stream_id,
                                                       int status) {
  FlipFrameBuilder frame;
  frame.WriteUInt16(kControlFlagMask | kFlipProtocolVersion);
  frame.WriteUInt16(FIN_STREAM);
  frame.WriteUInt32(8);
  frame.WriteUInt32(stream_id);
  frame.WriteUInt32(status);
  return reinterpret_cast<FlipFinStreamControlFrame*>(frame.take());
}

FlipSynReplyControlFrame* FlipFramer::CreateSynReply(FlipStreamId stream_id,
    FlipControlFlags flags, bool compressed, FlipHeaderBlock* headers) {

  FlipFrameBuilder frame;

  frame.WriteUInt16(kControlFlagMask | kFlipProtocolVersion);
  frame.WriteUInt16(SYN_REPLY);
  frame.WriteUInt32(0);  // Placeholder for the length and flags.
  frame.WriteUInt32(stream_id);
  frame.WriteUInt16(0);  // Unused

  frame.WriteUInt16(headers->size());  // Number of headers.
  FlipHeaderBlock::iterator it;
  for (it = headers->begin(); it != headers->end(); ++it) {
    // TODO(mbelshe): Headers need to be sorted.
    frame.WriteString(it->first);
    frame.WriteString(it->second);
  }

  // Write the length
  size_t length = frame.length() - FlipFrame::size();
  DCHECK(length < static_cast<size_t>(kLengthMask));
  FlagsAndLength flags_length;
  flags_length.length_ = htonl(static_cast<uint32>(length));
  flags_length.flags_[0] = flags;
  frame.WriteBytesToOffset(4, &flags_length, sizeof(flags_length));

  scoped_ptr<FlipFrame> reply_frame(frame.take());
  if (compressed) {
    return reinterpret_cast<FlipSynReplyControlFrame*>(
        CompressFrame(reply_frame.get()));
  }
  return reinterpret_cast<FlipSynReplyControlFrame*>(reply_frame.release());
}

FlipDataFrame* FlipFramer::CreateDataFrame(FlipStreamId stream_id,
                                           const char* data,
                                           uint32 len, FlipDataFlags flags) {
  FlipFrameBuilder frame;

  frame.WriteUInt32(stream_id);

  DCHECK(len < static_cast<size_t>(kLengthMask));
  FlagsAndLength flags_length;
  flags_length.length_ = htonl(len);
  flags_length.flags_[0] = flags;
  frame.WriteBytes(&flags_length, sizeof(flags_length));

  frame.WriteBytes(data, len);
  scoped_ptr<FlipFrame> data_frame(frame.take());
  if (flags & DATA_FLAG_COMPRESSED)
    return reinterpret_cast<FlipDataFrame*>(CompressFrame(data_frame.get()));
  return reinterpret_cast<FlipDataFrame*>(data_frame.release());
}

/* static */
FlipControlFrame* FlipFramer::CreateNopFrame() {
  FlipFrameBuilder frame;
  frame.WriteUInt16(kControlFlagMask | kFlipProtocolVersion);
  frame.WriteUInt16(NOOP);
  frame.WriteUInt32(0);
  return reinterpret_cast<FlipControlFrame*>(frame.take());
}

static const int kCompressorLevel = Z_DEFAULT_COMPRESSION;
// This is just a hacked dictionary to use for shrinking HTTP-like headers.
// TODO(mbelshe): Use a scientific methodology for computing the dictionary.
static const char dictionary[] =
  "optionsgetheadpostputdeletetraceacceptaccept-charsetaccept-encodingaccept-"
  "languageauthorizationexpectfromhostif-modified-sinceif-matchif-none-matchi"
  "f-rangeif-unmodifiedsincemax-forwardsproxy-authorizationrangerefererteuser"
  "-agent10010120020120220320420520630030130230330430530630740040140240340440"
  "5406407408409410411412413414415416417500501502503504505accept-rangesageeta"
  "glocationproxy-authenticatepublicretry-afterservervarywarningwww-authentic"
  "ateallowcontent-basecontent-encodingcache-controlconnectiondatetrailertran"
  "sfer-encodingupgradeviawarningcontent-languagecontent-lengthcontent-locati"
  "oncontent-md5content-rangecontent-typeetagexpireslast-modifiedset-cookieMo"
  "ndayTuesdayWednesdayThursdayFridaySaturdaySundayJanFebMarAprMayJunJulAugSe"
  "pOctNovDecchunkedtext/htmlimage/pngimage/jpgimage/gifapplication/xmlapplic"
  "ation/xhtmltext/plainpublicmax-agecharset=iso-8859-1utf-8gzipdeflateHTTP/1"
  ".1statusversionurl";
static uLong dictionary_id = 0;

bool FlipFramer::InitializeCompressor() {
  if (compressor_.get())
    return true;  // Already initialized.

  compressor_.reset(new z_stream);
  memset(compressor_.get(), 0, sizeof(z_stream));

  int success = deflateInit(compressor_.get(), kCompressorLevel);
  if (success == Z_OK)
    success = deflateSetDictionary(compressor_.get(),
                                   reinterpret_cast<const Bytef*>(dictionary),
                                   sizeof(dictionary));
  if (success != Z_OK)
    compressor_.reset(NULL);
  return success == Z_OK;
}

bool FlipFramer::InitializeDecompressor() {
  if (decompressor_.get())
    return true;  // Already initialized.

  decompressor_.reset(new z_stream);
  memset(decompressor_.get(), 0, sizeof(z_stream));

  // Compute the id of our dictionary so that we know we're using the
  // right one when asked for it.
  if (dictionary_id == 0) {
    dictionary_id = adler32(0L, Z_NULL, 0);
    dictionary_id = adler32(dictionary_id,
                            reinterpret_cast<const Bytef*>(dictionary),
                            sizeof(dictionary));
  }

  int success = inflateInit(decompressor_.get());
  if (success != Z_OK)
    decompressor_.reset(NULL);
  return success == Z_OK;
}

bool FlipFramer::GetFrameBoundaries(const FlipFrame* frame,
                                    int* payload_length,
                                    int* header_length,
                                    const char** payload) const {
  if (frame->is_control_frame()) {
    const FlipControlFrame* control_frame =
        reinterpret_cast<const FlipControlFrame*>(frame);
    switch (control_frame->type()) {
      case SYN_STREAM:
      case SYN_REPLY:
        {
          const FlipSynStreamControlFrame *syn_frame =
              reinterpret_cast<const FlipSynStreamControlFrame*>(frame);
          *payload_length = syn_frame->header_block_len();
          *header_length = syn_frame->size();
          *payload = frame->data() + *header_length;
        }
        break;
      default:
        // TODO(mbelshe): set an error?
        return false;  // We can't compress this frame!
    }
  } else {
    *header_length = FlipFrame::size();
    *payload_length = frame->length();
    *payload = frame->data() + FlipFrame::size();
  }
  DCHECK(static_cast<size_t>(*header_length) <=
      FlipFrame::size() + *payload_length);
  return true;
}


FlipFrame* FlipFramer::CompressFrame(const FlipFrame* frame) {
  int payload_length;
  int header_length;
  const char* payload;

  static StatsCounter pre_compress_bytes("flip.PreCompressSize");
  static StatsCounter post_compress_bytes("flip.PostCompressSize");

  if (!enable_compression_)
    return DuplicateFrame(frame);

  if (!GetFrameBoundaries(frame, &payload_length, &header_length, &payload))
    return NULL;

  if (!InitializeCompressor())
    return NULL;

  // TODO(mbelshe): Should we have a zlib header like what http servers do?

  // Create an output frame.
  int compressed_max_size = deflateBound(compressor_.get(), payload_length);
  int new_frame_size = header_length + compressed_max_size;
  FlipFrame* new_frame = new FlipFrame(new_frame_size);
  memcpy(new_frame->data(), frame->data(), frame->length() + FlipFrame::size());

  compressor_->next_in = reinterpret_cast<Bytef*>(const_cast<char*>(payload));
  compressor_->avail_in = payload_length;
  compressor_->next_out = reinterpret_cast<Bytef*>(new_frame->data()) +
                          header_length;
  compressor_->avail_out = compressed_max_size;

  // Data packets have a 'compressed flag
  if (!new_frame->is_control_frame()) {
    FlipDataFrame* data_frame = reinterpret_cast<FlipDataFrame*>(new_frame);
    data_frame->set_flags(data_frame->flags() | DATA_FLAG_COMPRESSED);
  }

  int rv = deflate(compressor_.get(), Z_SYNC_FLUSH);
  if (rv != Z_OK) {  // How can we know that it compressed everything?
    // This shouldn't happen, right?
    delete new_frame;
    return NULL;
  }

  int compressed_size = compressed_max_size - compressor_->avail_out;
  new_frame->set_length(header_length + compressed_size - FlipFrame::size());

  pre_compress_bytes.Add(payload_length);
  post_compress_bytes.Add(new_frame->length());

  return new_frame;
}

FlipFrame* FlipFramer::DecompressFrame(const FlipFrame* frame) {
  int payload_length;
  int header_length;
  const char* payload;

  static StatsCounter pre_decompress_bytes("flip.PreDeCompressSize");
  static StatsCounter post_decompress_bytes("flip.PostDeCompressSize");

  if (!enable_compression_)
    return DuplicateFrame(frame);

  if (!GetFrameBoundaries(frame, &payload_length, &header_length, &payload))
    return NULL;

  if (!frame->is_control_frame()) {
    const FlipDataFrame* data_frame =
        reinterpret_cast<const FlipDataFrame*>(frame);
    if ((data_frame->flags() & DATA_FLAG_COMPRESSED) == 0)
      return DuplicateFrame(frame);
  }

  if (!InitializeDecompressor())
    return NULL;

  // TODO(mbelshe): Should we have a zlib header like what http servers do?

  // Create an output frame.  Assume it does not need to be longer than
  // the input data.
  int decompressed_max_size = kControlFrameBufferInitialSize;
  int new_frame_size = header_length + decompressed_max_size;
  FlipFrame* new_frame = new FlipFrame(new_frame_size);
  memcpy(new_frame->data(), frame->data(), frame->length() + FlipFrame::size());

  decompressor_->next_in = reinterpret_cast<Bytef*>(const_cast<char*>(payload));
  decompressor_->avail_in = payload_length;
  decompressor_->next_out = reinterpret_cast<Bytef*>(new_frame->data()) +
      header_length;
  decompressor_->avail_out = decompressed_max_size;

  int rv = inflate(decompressor_.get(), Z_SYNC_FLUSH);
  if (rv == Z_NEED_DICT) {
    // Need to try again with the right dictionary.
    if (decompressor_->adler == dictionary_id) {
      rv = inflateSetDictionary(decompressor_.get(), (const Bytef*)dictionary,
                                sizeof(dictionary));
      if (rv == Z_OK)
        rv = inflate(decompressor_.get(), Z_SYNC_FLUSH);
    }
  }
  if (rv != Z_OK) {  // How can we know that it decompressed everything?
    delete new_frame;
    return NULL;
  }

  // Unset the compressed flag for data frames.
  if (!new_frame->is_control_frame()) {
    FlipDataFrame* data_frame = reinterpret_cast<FlipDataFrame*>(new_frame);
    data_frame->set_flags(data_frame->flags() & ~DATA_FLAG_COMPRESSED);
  }

  int decompressed_size = decompressed_max_size - decompressor_->avail_out;
  new_frame->set_length(header_length + decompressed_size - FlipFrame::size());

  pre_decompress_bytes.Add(frame->length());
  post_decompress_bytes.Add(new_frame->length());

  return new_frame;
}

FlipFrame* FlipFramer::DuplicateFrame(const FlipFrame* frame) {
  int size = FlipFrame::size() + frame->length();
  FlipFrame* new_frame = new FlipFrame(size);
  memcpy(new_frame->data(), frame->data(), size);
  return new_frame;
}

void FlipFramer::set_enable_compression(bool value) {
  enable_compression_ = value;
}

void FlipFramer::set_enable_compression_default(bool value) {
  compression_default_ = value;
}

}  // namespace flip

