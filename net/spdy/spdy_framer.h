// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_FRAMER_H_
#define NET_SPDY_SPDY_FRAMER_H_
#pragma once

#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/sys_byteorder.h"
#include "net/base/net_export.h"
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

class SpdyFramer;
class SpdyFrameBuilder;
class SpdyFramerTest;

namespace test {

class TestSpdyVisitor;

}  // namespace test

// A datastructure for holding a set of headers from either a
// SYN_STREAM or SYN_REPLY frame.
typedef std::map<std::string, std::string> SpdyHeaderBlock;

// A datastructure for holding the ID and flag fields for SETTINGS.
// Conveniently handles converstion to/from wire format.
class NET_EXPORT_PRIVATE SettingsFlagsAndId {
 public:
  static SettingsFlagsAndId FromWireFormat(int version, uint32 wire);

  SettingsFlagsAndId() : flags_(0), id_(0) {}

  // TODO(hkhalil): restrict to enums instead of free-form ints.
  SettingsFlagsAndId(uint8 flags, uint32 id);

  uint32 GetWireFormat(int version) const;

  uint32 id() const { return id_; }
  uint8 flags() const { return flags_; }

 private:
  static void ConvertFlagsAndIdForSpdy2(uint32* val);

  uint8 flags_;
  uint32 id_;
};

// SettingsMap has unique (flags, value) pair for given SpdySettingsIds ID.
typedef std::pair<SpdySettingsFlags, uint32> SettingsFlagsAndValue;
typedef std::map<SpdySettingsIds, SettingsFlagsAndValue> SettingsMap;

// A datastrcture for holding the contents of a CREDENTIAL frame.
struct NET_EXPORT_PRIVATE SpdyCredential {
  SpdyCredential();
  ~SpdyCredential();

  uint16 slot;
  std::vector<std::string> certs;
  std::string proof;
};

// Scratch space necessary for processing SETTINGS frames.
struct NET_EXPORT_PRIVATE SpdySettingsScratch {
  SpdySettingsScratch() { Reset(); }

  void Reset() {
    setting_buf_len = 0;
    last_setting_id = 0;
  }

  // Buffer contains up to one complete key/value pair.
  char setting_buf[8];

  // The amount of the buffer that is filled with valid data.
  size_t setting_buf_len;

  // The ID of the last setting that was processed in the current SETTINGS
  // frame. Used for detecting out-of-order or duplicate keys within a settings
  // frame. Set to 0 before first key/value pair is processed.
  uint32 last_setting_id;
};

// SpdyFramerVisitorInterface is a set of callbacks for the SpdyFramer.
// Implement this interface to receive event callbacks as frames are
// decoded from the framer.
//
// Control frames that contain SPDY header blocks (SYN_STREAM, SYN_REPLY, and
// HEADER) are processed in fashion that allows the decompressed header block
// to be delivered in chunks to the visitor. The following steps are followed:
//   1. OnControl is called, with either a SpdySynStreamControlFrame,
//      SpdySynReplyControlFrame, or a SpdyHeaderControlFrame argument.
//   2. Repeated: OnControlFrameHeaderData is called with chunks of the
//      decompressed header block. In each call the len parameter is greater
//      than zero.
//   3. OnControlFrameHeaderData is called with len set to zero, indicating
//      that the full header block has been delivered for the control frame.
// During step 2 the visitor may return false, indicating that the chunk of
// header data could not be handled by the visitor (typically this indicates
// resource exhaustion). If this occurs the framer will discontinue
// delivering chunks to the visitor, set a SPDY_CONTROL_PAYLOAD_TOO_LARGE
// error, and clean up appropriately. Note that this will cause the header
// decompressor to lose synchronization with the sender's header compressor,
// making the SPDY session unusable for future work. The visitor's OnError
// function should deal with this condition by closing the SPDY connection.
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

  // Called when a chunk of payload data for a credential frame is available.
  // This is called after OnControl() is called with the credential frame
  // associated with the payload being delivered here.
  // |header_data| A buffer containing the header data chunk received.
  // |len| The length of the header data buffer. A length of zero indicates
  //       that the header data block has been completely sent.
  // When this function returns true the visitor indicates that it accepted
  // all of the data. Returning false indicates that that an unrecoverable
  // error has occurred, such as bad header data or resource exhaustion.
  virtual bool OnCredentialFrameData(const char* header_data,
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

  // Called when a complete setting within a SETTINGS frame has been parsed and
  // validated.
  virtual void OnSetting(SpdySettingsIds id, uint8 flags, uint32 value) = 0;
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
    SPDY_CONTROL_FRAME_PAYLOAD,
    SPDY_IGNORE_REMAINING_PAYLOAD,
    SPDY_FORWARD_STREAM_FRAME,
    SPDY_CONTROL_FRAME_BEFORE_HEADER_BLOCK,
    SPDY_CONTROL_FRAME_HEADER_BLOCK,
    SPDY_CREDENTIAL_FRAME_PAYLOAD,
    SPDY_SETTINGS_FRAME_PAYLOAD,
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
    SPDY_CREDENTIAL_FRAME_CORRUPT,   // CREDENTIAL frame could not be parsed.
    SPDY_INVALID_DATA_FRAME_FLAGS,   // Data frame has invalid flags.

    LAST_ERROR,  // Must be the last entry in the enum.
  };

  // The minimum supported SPDY version that SpdyFramer can speak.
  static const int kMinSpdyVersion;

  // The maximum supported SPDY version that SpdyFramer can speak.
  static const int kMaxSpdyVersion;

  // Constant for invalid (or unknown) stream IDs.
  static const SpdyStreamId kInvalidStream;

  // The maximum size of header data chunks delivered to the framer visitor
  // through OnControlFrameHeaderData. (It is exposed here for unit test
  // purposes.)
  static const size_t kHeaderDataChunkMaxSize;

  // Create a new Framer, provided a SPDY version.
  explicit SpdyFramer(int version);
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

  bool MessageFullyRead() const {
    return state_ == SPDY_DONE || state_ == SPDY_AUTO_RESET;
  }
  bool HasError() const { return state_ == SPDY_ERROR; }

  // Given a buffer containing a decompressed header block in SPDY
  // serialized format, parse out a SpdyHeaderBlock, putting the results
  // in the given header block.
  // Returns true if successfully parsed, false otherwise.
  bool ParseHeaderBlockInBuffer(const char* header_data,
                                size_t header_length,
                                SpdyHeaderBlock* block) const;

  // Create a SpdySynStreamControlFrame.
  // |stream_id| is the id for this stream.
  // |associated_stream_id| is the associated stream id for this stream.
  // |priority| is the priority (GetHighestPriority()-GetLowestPriority) for
  //    this stream.
  // |credential_slot| is the CREDENTIAL slot to be used for this request.
  // |flags| is the flags to use with the data.
  //    To mark this frame as the last frame, enable CONTROL_FLAG_FIN.
  // |compressed| specifies whether the frame should be compressed.
  // |headers| is the header block to include in the frame.
  SpdySynStreamControlFrame* CreateSynStream(SpdyStreamId stream_id,
                                             SpdyStreamId associated_stream_id,
                                             SpdyPriority priority,
                                             uint8 credential_slot,
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

  SpdyRstStreamControlFrame* CreateRstStream(SpdyStreamId stream_id,
                                             SpdyStatusCodes status) const;

  // Creates an instance of SpdySettingsControlFrame. The SETTINGS frame is
  // used to communicate name/value pairs relevant to the communication channel.
  SpdySettingsControlFrame* CreateSettings(const SettingsMap& values) const;

  // Creates an instance of SpdyPingControlFrame. The unique_id is used to
  // identify the ping request/response.
  SpdyPingControlFrame* CreatePingFrame(uint32 unique_id) const;

  // Creates an instance of SpdyGoAwayControlFrame. The GOAWAY frame is used
  // prior to the shutting down of the TCP connection, and includes the
  // stream_id of the last stream the sender of the frame is willing to process
  // to completion.
  SpdyGoAwayControlFrame* CreateGoAway(SpdyStreamId last_accepted_stream_id,
                                       SpdyGoAwayStatus status) const;

  // Creates an instance of SpdyHeadersControlFrame. The HEADERS frame is used
  // for sending additional headers outside of a SYN_STREAM/SYN_REPLY. The
  // arguments are the same as for CreateSynReply.
  SpdyHeadersControlFrame* CreateHeaders(SpdyStreamId stream_id,
                                         SpdyControlFlags flags,
                                         bool compressed,
                                         const SpdyHeaderBlock* headers);

  // Creates an instance of SpdyWindowUpdateControlFrame. The WINDOW_UPDATE
  // frame is used to implement per stream flow control in SPDY.
  SpdyWindowUpdateControlFrame* CreateWindowUpdate(
      SpdyStreamId stream_id,
      uint32 delta_window_size) const;

  // Creates an instance of SpdyCredentialControlFrame.  The CREDENTIAL
  // frame is used to send a client certificate to the server when
  // request more than one origin are sent over the same SPDY session.
  SpdyCredentialControlFrame* CreateCredentialFrame(
      const SpdyCredential& credential) const;

  // Given a SpdySettingsControlFrame, extract the settings.
  // Returns true on successful parse, false otherwise.
  static bool ParseSettings(const SpdySettingsControlFrame* frame,
                            SettingsMap* settings);

  // Given a SpdyCredentialControlFrame's payload, extract the credential.
  // Returns true on successful parse, false otherwise.
  // TODO(hkhalil): Implement CREDENTIAL frame parsing in SpdyFramer
  // and eliminate this method.
  static bool ParseCredentialData(const char* data, size_t len,
                                  SpdyCredential* credential);

  // Create a data frame.
  // |stream_id| is the stream  for this frame
  // |data| is the data to be included in the frame.
  // |len| is the length of the data
  // |flags| is the flags to use with the data.
  //    To mark this frame as the last data frame, enable DATA_FLAG_FIN.
  SpdyDataFrame* CreateDataFrame(SpdyStreamId stream_id, const char* data,
                                 uint32 len, SpdyDataFlags flags) const;

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

  // Create a copy of a frame.
  // Returned frame must be freed with "delete".
  SpdyFrame* DuplicateFrame(const SpdyFrame& frame);

  // Returns true if a frame could be compressed.
  bool IsCompressible(const SpdyFrame& frame) const;

  // Returns a new SpdyControlFrame with the compressed payload of |frame|.
  SpdyControlFrame* CompressControlFrame(const SpdyControlFrame& frame);

  // Get the minimum size of the control frame for the given control frame
  // type. This is useful for validating frame blocks.
  static size_t GetMinimumControlFrameSize(int version, SpdyControlType type);

  // Get the stream ID for the given control frame (SYN_STREAM, SYN_REPLY, and
  // HEADERS). If the control frame is NULL or of another type, this
  // function returns kInvalidStream.
  static SpdyStreamId GetControlFrameStreamId(
      const SpdyControlFrame* control_frame);

  // For ease of testing and experimentation we can tweak compression on/off.
  void set_enable_compression(bool value);

  // Used only in log messages.
  void set_display_protocol(const std::string& protocol) {
    display_protocol_ = protocol;
  }

  // For debugging.
  static const char* StateToString(int state);
  static const char* ErrorCodeToString(int error_code);
  static const char* StatusCodeToString(int status_code);
  static const char* ControlTypeToString(SpdyControlType type);

  int protocol_version() const { return spdy_version_; }

  bool probable_http_response() const { return probable_http_response_; }

  SpdyPriority GetLowestPriority() const { return spdy_version_ < 3 ? 3 : 7; }
  SpdyPriority GetHighestPriority() const { return 0; }

 protected:
  FRIEND_TEST_ALL_PREFIXES(SpdyFramerTest, BasicCompression);
  FRIEND_TEST_ALL_PREFIXES(SpdyFramerTest, ControlFrameSizesAreValidated);
  FRIEND_TEST_ALL_PREFIXES(SpdyFramerTest, HeaderCompression);
  FRIEND_TEST_ALL_PREFIXES(SpdyFramerTest, DecompressUncompressedFrame);
  FRIEND_TEST_ALL_PREFIXES(SpdyFramerTest, ExpandBuffer_HeapSmash);
  FRIEND_TEST_ALL_PREFIXES(SpdyFramerTest, HugeHeaderBlock);
  FRIEND_TEST_ALL_PREFIXES(SpdyFramerTest, UnclosedStreamDataCompressors);
  FRIEND_TEST_ALL_PREFIXES(SpdyFramerTest,
                           UnclosedStreamDataCompressorsOneByteAtATime);
  FRIEND_TEST_ALL_PREFIXES(SpdyFramerTest,
                           UncompressLargerThanFrameBufferInitialSize);
  FRIEND_TEST_ALL_PREFIXES(SpdyFramerTest, ReadLargeSettingsFrame);
  FRIEND_TEST_ALL_PREFIXES(SpdyFramerTest,
                           ReadLargeSettingsFrameInSmallChunks);
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

 private:
  // Internal breakouts from ProcessInput. Each returns the number of bytes
  // consumed from the data.
  size_t ProcessCommonHeader(const char* data, size_t len);
  size_t ProcessControlFramePayload(const char* data, size_t len);
  size_t ProcessCredentialFramePayload(const char* data, size_t len);
  size_t ProcessControlFrameBeforeHeaderBlock(const char* data, size_t len);
  size_t ProcessControlFrameHeaderBlock(const char* data, size_t len);
  size_t ProcessSettingsFramePayload(const char* data, size_t len);
  size_t ProcessDataFramePayload(const char* data, size_t len);

  // Helpers for above internal breakouts from ProcessInput.
  void ProcessControlFrameHeader();
  bool ProcessSetting(const char* data);  // Always passed exactly 8 bytes.

  // Get (and lazily initialize) the ZLib state.
  z_stream* GetHeaderCompressor();
  z_stream* GetHeaderDecompressor();

  // Deliver the given control frame's compressed headers block to the visitor
  // in decompressed form, in chunks. Returns true if the visitor has
  // accepted all of the chunks.
  bool IncrementallyDecompressControlFrameHeaderData(
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

  // Retrieve serialized length of SpdyHeaderBlock.
  size_t GetSerializedLength(const SpdyHeaderBlock* headers) const;

  // Serializes a SpdyHeaderBlock.
  void WriteHeaderBlock(SpdyFrameBuilder* frame,
                        const SpdyHeaderBlock* headers) const;

  // Set the error code and moves the framer into the error state.
  void set_error(SpdyError error);

  // Given a frame, breakdown the variable payload length, the static header
  // header length, and variable payload pointer.
  bool GetFrameBoundaries(const SpdyFrame& frame, int* payload_length,
                          int* header_length, const char** payload) const;

  // The size of the control frame buffer.
  // Since this is only used for control frame headers, the maximum control
  // frame header size (SYN_STREAM) is sufficient; all remaining control
  // frame data is streamed to the visitor.
  static const size_t kControlFrameBufferSize;

  // The maximum size of the control frames that we support.
  // This limit is arbitrary. We can enforce it here or at the application
  // layer. We chose the framing layer, but this can be changed (or removed)
  // if necessary later down the line.
  static const size_t kMaxControlFrameSize;

  SpdyState state_;
  SpdyState previous_state_;
  SpdyError error_code_;
  size_t remaining_data_;

  // The number of bytes remaining to read from the current control frame's
  // payload.
  size_t remaining_control_payload_;

  // The number of bytes remaining to read from the current control frame's
  // headers. Note that header data blocks (for control types that have them)
  // are part of the frame's payload, and not the frame's headers.
  size_t remaining_control_header_;

  scoped_array<char> current_frame_buffer_;
  size_t current_frame_len_;  // Number of bytes read into the current_frame_.

  // Scratch space for handling SETTINGS frames.
  // TODO(hkhalil): Unify memory for this scratch space with
  // current_frame_buffer_.
  SpdySettingsScratch settings_scratch_;

  bool enable_compression_;  // Controls all compression
  // SPDY header compressors.
  scoped_ptr<z_stream> header_compressor_;
  scoped_ptr<z_stream> header_decompressor_;

  SpdyFramerVisitorInterface* visitor_;

  std::string display_protocol_;

  // The SPDY version to be spoken/understood by this framer. We support only
  // integer versions here, as major version numbers indicate framer-layer
  // incompatibility and minor version numbers indicate application-layer
  // incompatibility.
  const int spdy_version_;

  // Tracks if we've ever gotten far enough in framing to see a control frame of
  // type SYN_STREAM or SYN_REPLY.
  //
  // If we ever get something which looks like a data frame before we've had a
  // SYN, we explicitly check to see if it looks like we got an HTTP response to
  // a SPDY request.  This boolean lets us do that.
  bool syn_frame_processed_;

  // If we ever get a data frame before a SYN frame, we check to see if it
  // starts with HTTP.  If it does, we likely have an HTTP response.   This
  // isn't guaranteed though: we could have gotten a settings frame and then
  // corrupt data that just looks like HTTP, but deterministic checking requires
  // a lot more state.
  bool probable_http_response_;
};

}  // namespace net

#endif  // NET_SPDY_SPDY_FRAMER_H_
