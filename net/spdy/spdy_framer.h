// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_FRAMER_H_
#define NET_SPDY_SPDY_FRAMER_H_
#pragma once

#include <list>
#include <map>
#include <string>
#include <utility>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"
#include "net/base/sys_byteorder.h"
#include "net/spdy/spdy_protocol.h"

typedef struct z_stream_s z_stream;  // Forward declaration for zlib.

namespace net {
class HttpProxyClientSocketPoolTest;
class HttpNetworkLayer;
class HttpNetworkTransactionTest;
class SpdyHttpStreamTest;
class SpdyNetworkTransactionTest;
class SpdyProxyClientSocketTest;
class SpdySessionTest;
class SpdyStreamTest;
class SpdyWebSocketStreamTest;
class WebSocketJobTest;
}

namespace spdy {

class SpdyFramer;
class SpdyFramerTest;

namespace test {
class TestSpdyVisitor;
void FramerSetEnableCompressionHelper(SpdyFramer* framer, bool compress);
}  // namespace test

// A datastructure for holding a set of headers from either a
// SYN_STREAM or SYN_REPLY frame.
typedef std::map<std::string, std::string> SpdyHeaderBlock;

// A datastructure for holding a set of ID/value pairs for a SETTINGS frame.
typedef std::pair<spdy::SettingsFlagsAndId, uint32> SpdySetting;
typedef std::list<SpdySetting> SpdySettings;

// SpdyFramerVisitorInterface is a set of callbacks for the SpdyFramer.
// Implement this interface to receive event callbacks as frames are
// decoded from the framer.
class NET_EXPORT_PRIVATE SpdyFramerVisitorInterface {
 public:
  virtual ~SpdyFramerVisitorInterface() {}

  // Called if an error is detected in the SpdyFrame protocol.
  virtual void OnError(SpdyFramer* framer) = 0;

  // Called when a control frame is received. Note that SYN_STREAM, SYN_REPLY,
  // and HEADER control frames do not include the header block data.
  // (See OnControlFrameHeaderData().)
  virtual void OnControl(const SpdyControlFrame* frame) = 0;

  // Called when a chunk of header data is available. This is called
  // after OnControl() is called with the control frame associated with the
  // header data being delivered here.
  // |stream_id| The stream receiving the header data.
  // |header_data| A buffer containing the header data chunk received.
  // |len| The length of the header data buffer. A length of zero indicates
  //       that the header data block has been completely sent.
  // When this function returns true the visitor indicates that it accepted
  // all of the data. Returning false indicates that that an unrecoverable
  // error has occurred, such as bad header data or resource exhaustion.
  virtual bool OnControlFrameHeaderData(SpdyStreamId stream_id,
                                        const char* header_data,
                                        size_t len) = 0;

  // Called when a data frame header is received. The frame's data
  // payload will be provided via subsequent calls to
  // OnStreamFrameData().
  virtual void OnDataFrameHeader(const SpdyDataFrame* frame) = 0;

  // Called when data is received.
  // |stream_id| The stream receiving data.
  // |data| A buffer containing the data received.
  // |len| The length of the data buffer.
  // When the other side has finished sending data on this stream,
  // this method will be called with a zero-length buffer.
  virtual void OnStreamFrameData(SpdyStreamId stream_id,
                                 const char* data,
                                 size_t len) = 0;
};

class NET_EXPORT_PRIVATE SpdyFramer {
 public:
  // SPDY states.
  // TODO(mbelshe): Can we move these into the implementation
  //                and avoid exposing through the header.  (Needed for test)
  enum SpdyState {
    SPDY_ERROR,
    SPDY_DONE,
    SPDY_RESET,
    SPDY_AUTO_RESET,
    SPDY_READING_COMMON_HEADER,
    SPDY_INTERPRET_CONTROL_FRAME_COMMON_HEADER,
    SPDY_CONTROL_FRAME_PAYLOAD,
    SPDY_IGNORE_REMAINING_PAYLOAD,
    SPDY_FORWARD_STREAM_FRAME,
    SPDY_CONTROL_FRAME_BEFORE_HEADER_BLOCK,
    SPDY_CONTROL_FRAME_HEADER_BLOCK,
  };

  // SPDY error codes.
  enum SpdyError {
    SPDY_NO_ERROR,
    SPDY_INVALID_CONTROL_FRAME,      // Control frame is mal-formatted.
    SPDY_CONTROL_PAYLOAD_TOO_LARGE,  // Control frame payload was too large.
    SPDY_ZLIB_INIT_FAILURE,          // The Zlib library could not initialize.
    SPDY_UNSUPPORTED_VERSION,        // Control frame has unsupported version.
    SPDY_DECOMPRESS_FAILURE,         // There was an error decompressing.
    SPDY_COMPRESS_FAILURE,           // There was an error compressing.

    LAST_ERROR,  // Must be the last entry in the enum.
  };

  // Constant for invalid (or unknown) stream IDs.
  static const SpdyStreamId kInvalidStream;

  // The maximum size of header data chunks delivered to the framer visitor
  // through OnControlFrameHeaderData. (It is exposed here for unit test
  // purposes.)
  static const size_t kHeaderDataChunkMaxSize;

  // Create a new Framer.
  SpdyFramer();
  virtual ~SpdyFramer();

  // Set callbacks to be called from the framer.  A visitor must be set, or
  // else the framer will likely crash.  It is acceptable for the visitor
  // to do nothing.  If this is called multiple times, only the last visitor
  // will be used.
  void set_visitor(SpdyFramerVisitorInterface* visitor) {
    visitor_ = visitor;
  }

  // Pass data into the framer for parsing.
  // Returns the number of bytes consumed. It is safe to pass more bytes in
  // than may be consumed.
  size_t ProcessInput(const char* data, size_t len);

  // Resets the framer state after a frame has been successfully decoded.
  // TODO(mbelshe): can we make this private?
  void Reset();

  // Check the state of the framer.
  SpdyError error_code() const { return error_code_; }
  SpdyState state() const { return state_; }

  bool MessageFullyRead() {
    return state_ == SPDY_DONE || state_ == SPDY_AUTO_RESET;
  }
  bool HasError() { return state_ == SPDY_ERROR; }

  // Further parsing utilities.
  // Given a control frame, parse out a SpdyHeaderBlock.  Only
  // valid for SYN_STREAM and SYN_REPLY frames.
  // Returns true if successfully parsed, false otherwise.
  bool ParseHeaderBlock(const SpdyFrame* frame, SpdyHeaderBlock* block);

  // Given a buffer containing a decompressed header block in SPDY
  // serialized format, parse out a SpdyHeaderBlock, putting the results
  // in the given header block.
  // Returns true if successfully parsed, false otherwise.
  static bool ParseHeaderBlockInBuffer(const char* header_data,
                                       size_t header_length,
                                       SpdyHeaderBlock* block);

  // Create a SpdySynStreamControlFrame.
  // |stream_id| is the id for this stream.
  // |associated_stream_id| is the associated stream id for this stream.
  // |priority| is the priority (0-3) for this stream.
  // |flags| is the flags to use with the data.
  //    To mark this frame as the last frame, enable CONTROL_FLAG_FIN.
  // |compressed| specifies whether the frame should be compressed.
  // |headers| is the header block to include in the frame.
  SpdySynStreamControlFrame* CreateSynStream(SpdyStreamId stream_id,
                                             SpdyStreamId associated_stream_id,
                                             int priority,
                                             SpdyControlFlags flags,
                                             bool compressed,
                                             const SpdyHeaderBlock* headers);

  // Create a SpdySynReplyControlFrame.
  // |stream_id| is the stream for this frame.
  // |flags| is the flags to use with the data.
  //    To mark this frame as the last frame, enable CONTROL_FLAG_FIN.
  // |compressed| specifies whether the frame should be compressed.
  // |headers| is the header block to include in the frame.
  SpdySynReplyControlFrame* CreateSynReply(SpdyStreamId stream_id,
                                           SpdyControlFlags flags,
                                           bool compressed,
                                           const SpdyHeaderBlock* headers);

  static SpdyRstStreamControlFrame* CreateRstStream(SpdyStreamId stream_id,
                                                    SpdyStatusCodes status);

  // Creates an instance of SpdySettingsControlFrame. The SETTINGS frame is
  // used to communicate name/value pairs relevant to the communication channel.
  // TODO(mbelshe): add the name/value pairs!!
  static SpdySettingsControlFrame* CreateSettings(const SpdySettings& values);

  static SpdyNoOpControlFrame* CreateNopFrame();

  // Creates an instance of SpdyPingControlFrame. The unique_id is used to
  // identify the ping request/response.
  static SpdyPingControlFrame* CreatePingFrame(uint32 unique_id);

  // Creates an instance of SpdyGoAwayControlFrame. The GOAWAY frame is used
  // prior to the shutting down of the TCP connection, and includes the
  // stream_id of the last stream the sender of the frame is willing to process
  // to completion.
  static SpdyGoAwayControlFrame* CreateGoAway(
      SpdyStreamId last_accepted_stream_id);

  // Creates an instance of SpdyHeadersControlFrame. The HEADERS frame is used
  // for sending additional headers outside of a SYN_STREAM/SYN_REPLY. The
  // arguments are the same as for CreateSynReply.
  SpdyHeadersControlFrame* CreateHeaders(SpdyStreamId stream_id,
                                         SpdyControlFlags flags,
                                         bool compressed,
                                         const SpdyHeaderBlock* headers);

  // Creates an instance of SpdyWindowUpdateControlFrame. The WINDOW_UPDATE
  // frame is used to implement per stream flow control in SPDY.
  static SpdyWindowUpdateControlFrame* CreateWindowUpdate(
      SpdyStreamId stream_id,
      uint32 delta_window_size);

  // Given a SpdySettingsControlFrame, extract the settings.
  // Returns true on successful parse, false otherwise.
  static bool ParseSettings(const SpdySettingsControlFrame* frame,
      SpdySettings* settings);

  // Create a data frame.
  // |stream_id| is the stream  for this frame
  // |data| is the data to be included in the frame.
  // |len| is the length of the data
  // |flags| is the flags to use with the data.
  //    To create a compressed frame, enable DATA_FLAG_COMPRESSED.
  //    To mark this frame as the last data frame, enable DATA_FLAG_FIN.
  SpdyDataFrame* CreateDataFrame(SpdyStreamId stream_id, const char* data,
                                 uint32 len, SpdyDataFlags flags);

  // NOTES about frame compression.
  // We want spdy to compress headers across the entire session.  As long as
  // the session is over TCP, frames are sent serially.  The client & server
  // can each compress frames in the same order and then compress them in that
  // order, and the remote can do the reverse.  However, we ultimately want
  // the creation of frames to be less sensitive to order so that they can be
  // placed over a UDP based protocol and yet still benefit from some
  // compression.  We don't know of any good compression protocol which does
  // not build its state in a serial (stream based) manner....  For now, we're
  // using zlib anyway.

  // Compresses a SpdyFrame.
  // On success, returns a new SpdyFrame with the payload compressed.
  // Compression state is maintained as part of the SpdyFramer.
  // Returned frame must be freed with "delete".
  // On failure, returns NULL.
  SpdyFrame* CompressFrame(const SpdyFrame& frame);

  // Decompresses a SpdyFrame.
  // On success, returns a new SpdyFrame with the payload decompressed.
  // Compression state is maintained as part of the SpdyFramer.
  // Returned frame must be freed with "delete".
  // On failure, returns NULL.
  SpdyFrame* DecompressFrame(const SpdyFrame& frame);

  // Create a copy of a frame.
  // Returned frame must be freed with "delete".
  SpdyFrame* DuplicateFrame(const SpdyFrame& frame);

  // Returns true if a frame could be compressed.
  bool IsCompressible(const SpdyFrame& frame) const;

  // Get the minimum size of the control frame for the given control frame
  // type. This is useful for validating frame blocks.
  static size_t GetMinimumControlFrameSize(SpdyControlType type);

  // Get the stream ID for the given control frame (SYN_STREAM, SYN_REPLY, and
  // HEADERS). If the control frame is NULL or of another type, this
  // function returns kInvalidStream.
  static SpdyStreamId GetControlFrameStreamId(
      const SpdyControlFrame* control_frame);

  // For ease of testing and experimentation we can tweak compression on/off.
  void set_enable_compression(bool value);

  // SPDY will by default validate the length of incoming control
  // frames. Set validation to false if you do not want this behavior.
  void set_validate_control_frame_sizes(bool value);
  static void set_enable_compression_default(bool value);

  // Used only in log messages.
  void set_display_protocol(const std::string& protocol) {
    display_protocol_ = protocol;
  }

  // For debugging.
  static const char* StateToString(int state);
  static const char* ErrorCodeToString(int error_code);
  static const char* StatusCodeToString(int status_code);
  static const char* ControlTypeToString(SpdyControlType type);

  static void set_protocol_version(int version) { spdy_version_= version; }
  static int protocol_version() { return spdy_version_; }

  // Export the compression dictionary
  static const char kDictionary[];
  static const int kDictionarySize;

 protected:
  FRIEND_TEST_ALL_PREFIXES(SpdyFramerTest, DataCompression);
  FRIEND_TEST_ALL_PREFIXES(SpdyFramerTest, ExpandBuffer_HeapSmash);
  FRIEND_TEST_ALL_PREFIXES(SpdyFramerTest, HugeHeaderBlock);
  FRIEND_TEST_ALL_PREFIXES(SpdyFramerTest, UnclosedStreamDataCompressors);
  friend class net::HttpNetworkLayer;  // This is temporary for the server.
  friend class net::HttpNetworkTransactionTest;
  friend class net::HttpProxyClientSocketPoolTest;
  friend class net::SpdyHttpStreamTest;
  friend class net::SpdyNetworkTransactionTest;
  friend class net::SpdyProxyClientSocketTest;
  friend class net::SpdySessionTest;
  friend class net::SpdyStreamTest;
  friend class net::SpdyWebSocketStreamTest;
  friend class net::WebSocketJobTest;
  friend class test::TestSpdyVisitor;
  friend void test::FramerSetEnableCompressionHelper(SpdyFramer* framer,
                                                     bool compress);

 private:
  typedef std::map<SpdyStreamId, z_stream*> CompressorMap;

  // Internal breakout from ProcessInput.  Returns the number of bytes
  // consumed from the data.
  size_t ProcessCommonHeader(const char* data, size_t len);
  void ProcessControlFrameHeader();
  size_t ProcessControlFramePayload(const char* data, size_t len);
  size_t ProcessControlFrameBeforeHeaderBlock(const char* data, size_t len);
  size_t NewProcessControlFrameHeaderBlock(const char* data, size_t len);
  size_t ProcessControlFrameHeaderBlock(const char* data, size_t len);
  size_t ProcessDataFramePayload(const char* data, size_t len);

  // Get (and lazily initialize) the ZLib state.
  z_stream* GetHeaderCompressor();
  z_stream* GetHeaderDecompressor();
  z_stream* GetStreamCompressor(SpdyStreamId id);
  z_stream* GetStreamDecompressor(SpdyStreamId id);

  // Compression helpers
  SpdyControlFrame* CompressControlFrame(const SpdyControlFrame& frame);
  SpdyDataFrame* CompressDataFrame(const SpdyDataFrame& frame);
  SpdyControlFrame* DecompressControlFrame(const SpdyControlFrame& frame);
  SpdyDataFrame* DecompressDataFrame(const SpdyDataFrame& frame);
  SpdyFrame* CompressFrameWithZStream(const SpdyFrame& frame,
                                      z_stream* compressor);
  SpdyFrame* DecompressFrameWithZStream(const SpdyFrame& frame,
                                        z_stream* decompressor);
  void CleanupCompressorForStream(SpdyStreamId id);
  void CleanupDecompressorForStream(SpdyStreamId id);
  void CleanupStreamCompressorsAndDecompressors();

  // Not used (yet)
  size_t BytesSafeToRead() const;

  // Deliver the given control frame's compressed headers block to the visitor
  // in decompressed form, in chunks. Returns true if the visitor has
  // accepted all of the chunks.
  bool IncrementallyDecompressControlFrameHeaderData(
      const SpdyControlFrame* frame);

  // Deliver the given control frame's compressed headers block to the visitor
  // in decompressed form, in chunks. Returns true if the visitor has
  // accepted all of the chunks.
  bool NewIncrementallyDecompressControlFrameHeaderData(
      const SpdyControlFrame* frame,
      const char* data,
      size_t len);

  // Deliver the given control frame's uncompressed headers block to the
  // visitor in chunks. Returns true if the visitor has accepted all of the
  // chunks.
  bool IncrementallyDeliverControlFrameHeaderData(const SpdyControlFrame* frame,
                                                  const char* data,
                                                  size_t len);

  // Utility to copy the given data block to the current frame buffer, up
  // to the given maximum number of bytes, and update the buffer
  // data (pointer and length). Returns the number of bytes
  // read, and:
  //   *data is advanced the number of bytes read.
  //   *len is reduced by the number of bytes read.
  size_t UpdateCurrentFrameBuffer(const char** data, size_t* len,
                                  size_t max_bytes);

  // Set the error code and moves the framer into the error state.
  void set_error(SpdyError error);

  // Expands the control frame buffer to accomodate a particular payload size.
  void ExpandControlFrameBuffer(size_t size);

  // Given a frame, breakdown the variable payload length, the static header
  // header length, and variable payload pointer.
  bool GetFrameBoundaries(const SpdyFrame& frame, int* payload_length,
                          int* header_length, const char** payload) const;

  int num_stream_compressors() const { return stream_compressors_.size(); }
  int num_stream_decompressors() const { return stream_decompressors_.size(); }

  // The initial size of the control frame buffer; this is used internally
  // as we parse through control frames. (It is exposed here for unit test
  // purposes.)
  // This is only used when compression is enabled; otherwise,
  // kUncompressedControlFrameBufferInitialSize is used.
  static size_t kControlFrameBufferInitialSize;

  // The initial size of the control frame buffer when compression is disabled.
  // This exists because we don't do stream (de)compressed control frame data to
  // our visitor; we instead buffer the entirety of the control frame and then
  // decompress in one fell swoop.
  // Since this is only used for control frame headers, the maximum control
  // frame header size (18B) is sufficient; all remaining control frame data is
  // streamed to the visitor.
  // In addition to the maximum control frame header size, we account for any
  // LOAS checksumming (16B) that may occur in the VTL case.
  // TODO(hkhalil): Remove post code-yellow once streamed inflate is properly
  // implemented.
  static const size_t kUncompressedControlFrameBufferInitialSize = 18 + 16;

  // The maximum size of the control frame buffer that we support.
  // TODO(mbelshe): We should make this stream-based so there are no limits.
  static size_t kControlFrameBufferMaxSize;

  // The size of the buffer into which compressed frames are inflated.
  static const size_t kDecompressionBufferSize = 8 * 1024;

  SpdyState state_;
  SpdyError error_code_;
  size_t remaining_data_;

  // The number of bytes remaining to read from the current control frame's
  // payload.
  size_t remaining_control_payload_;

  // The number of bytes remaining to read from the current control frame's
  // headers. Note that header data blocks (for control types that have them)
  // are part of the frame's payload, and not the frame's headers.
  size_t remaining_control_header_;

  char* current_frame_buffer_;
  size_t current_frame_len_;  // Number of bytes read into the current_frame_.
  size_t current_frame_capacity_;

  bool validate_control_frame_sizes_;
  bool enable_compression_;  // Controls all compression
  // SPDY header compressors.
  scoped_ptr<z_stream> header_compressor_;
  scoped_ptr<z_stream> header_decompressor_;

  // Per-stream data compressors.
  CompressorMap stream_compressors_;
  CompressorMap stream_decompressors_;

  SpdyFramerVisitorInterface* visitor_;

  std::string display_protocol_;

  static bool compression_default_;
  static int spdy_version_;
};

}  // namespace spdy

#endif  // NET_SPDY_SPDY_FRAMER_H_
