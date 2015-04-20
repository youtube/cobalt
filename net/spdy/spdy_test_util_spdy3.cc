// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_test_util_spdy3.h"

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "net/base/mock_cert_verifier.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_server_properties_impl.h"
#include "net/spdy/buffered_spdy_framer.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_session.h"

namespace net {

namespace test_spdy3 {

namespace {

// Parses a URL into the scheme, host, and path components required for a
// SPDY request.
void ParseUrl(const char* const url, std::string* scheme, std::string* host,
              std::string* path) {
  GURL gurl(url);
  path->assign(gurl.PathForRequest());
  scheme->assign(gurl.scheme());
  host->assign(gurl.host());
  if (gurl.has_port()) {
    host->append(":");
    host->append(gurl.port());
  }
}

}  // namespace


MockECSignatureCreator::MockECSignatureCreator(crypto::ECPrivateKey* key)
    : key_(key) {
}

bool MockECSignatureCreator::Sign(const uint8* data,
                                  int data_len,
                                  std::vector<uint8>* signature) {
  std::vector<uint8> private_key_value;
  key_->ExportValue(&private_key_value);
  std::string head = "fakesignature";
  std::string tail = "/fakesignature";

  signature->clear();
  signature->insert(signature->end(), head.begin(), head.end());
  signature->insert(signature->end(), private_key_value.begin(),
                    private_key_value.end());
  signature->insert(signature->end(), '-');
  signature->insert(signature->end(), data, data + data_len);
  signature->insert(signature->end(), tail.begin(), tail.end());
  return true;
}

bool MockECSignatureCreator::DecodeSignature(
    const std::vector<uint8>& signature,
    std::vector<uint8>* out_raw_sig) {
  *out_raw_sig = signature;
  return true;
}

MockECSignatureCreatorFactory::MockECSignatureCreatorFactory() {
  crypto::ECSignatureCreator::SetFactoryForTesting(this);
}

MockECSignatureCreatorFactory::~MockECSignatureCreatorFactory() {
  crypto::ECSignatureCreator::SetFactoryForTesting(NULL);
}

crypto::ECSignatureCreator* MockECSignatureCreatorFactory::Create(
    crypto::ECPrivateKey* key) {
  return new MockECSignatureCreator(key);
}

// Chop a frame into an array of MockWrites.
// |data| is the frame to chop.
// |length| is the length of the frame to chop.
// |num_chunks| is the number of chunks to create.
MockWrite* ChopWriteFrame(const char* data, int length, int num_chunks) {
  MockWrite* chunks = new MockWrite[num_chunks];
  int chunk_size = length / num_chunks;
  for (int index = 0; index < num_chunks; index++) {
    const char* ptr = data + (index * chunk_size);
    if (index == num_chunks - 1)
      chunk_size += length % chunk_size;  // The last chunk takes the remainder.
    chunks[index] = MockWrite(ASYNC, ptr, chunk_size);
  }
  return chunks;
}

// Chop a SpdyFrame into an array of MockWrites.
// |frame| is the frame to chop.
// |num_chunks| is the number of chunks to create.
MockWrite* ChopWriteFrame(const SpdyFrame& frame, int num_chunks) {
  return ChopWriteFrame(frame.data(),
                        frame.length() + SpdyFrame::kHeaderSize,
                        num_chunks);
}

// Chop a frame into an array of MockReads.
// |data| is the frame to chop.
// |length| is the length of the frame to chop.
// |num_chunks| is the number of chunks to create.
MockRead* ChopReadFrame(const char* data, int length, int num_chunks) {
  MockRead* chunks = new MockRead[num_chunks];
  int chunk_size = length / num_chunks;
  for (int index = 0; index < num_chunks; index++) {
    const char* ptr = data + (index * chunk_size);
    if (index == num_chunks - 1)
      chunk_size += length % chunk_size;  // The last chunk takes the remainder.
    chunks[index] = MockRead(ASYNC, ptr, chunk_size);
  }
  return chunks;
}

// Chop a SpdyFrame into an array of MockReads.
// |frame| is the frame to chop.
// |num_chunks| is the number of chunks to create.
MockRead* ChopReadFrame(const SpdyFrame& frame, int num_chunks) {
  return ChopReadFrame(frame.data(),
                       frame.length() + SpdyFrame::kHeaderSize,
                       num_chunks);
}

// Adds headers and values to a map.
// |extra_headers| is an array of { name, value } pairs, arranged as strings
// where the even entries are the header names, and the odd entries are the
// header values.
// |headers| gets filled in from |extra_headers|.
void AppendHeadersToSpdyFrame(const char* const extra_headers[],
                              int extra_header_count,
                              SpdyHeaderBlock* headers) {
  std::string this_header;
  std::string this_value;

  if (!extra_header_count)
    return;

  // Sanity check: Non-NULL header list.
  DCHECK(NULL != extra_headers) << "NULL header value pair list";
  // Sanity check: Non-NULL header map.
  DCHECK(NULL != headers) << "NULL header map";
  // Copy in the headers.
  for (int i = 0; i < extra_header_count; i++) {
    // Sanity check: Non-empty header.
    DCHECK_NE('\0', *extra_headers[i * 2]) << "Empty header value pair";
    this_header = extra_headers[i * 2];
    std::string::size_type header_len = this_header.length();
    if (!header_len)
      continue;
    this_value = extra_headers[1 + (i * 2)];
    std::string new_value;
    if (headers->find(this_header) != headers->end()) {
      // More than one entry in the header.
      // Don't add the header again, just the append to the value,
      // separated by a NULL character.

      // Adjust the value.
      new_value = (*headers)[this_header];
      // Put in a NULL separator.
      new_value.append(1, '\0');
      // Append the new value.
      new_value += this_value;
    } else {
      // Not a duplicate, just write the value.
      new_value = this_value;
    }
    (*headers)[this_header] = new_value;
  }
}

// Writes |val| to a location of size |len|, in big-endian format.
// in the buffer pointed to by |buffer_handle|.
// Updates the |*buffer_handle| pointer by |len|
// Returns the number of bytes written
int AppendToBuffer(int val,
                   int len,
                   unsigned char** buffer_handle,
                   int* buffer_len_remaining) {
  if (len <= 0)
    return 0;
  DCHECK((size_t) len <= sizeof(len)) << "Data length too long for data type";
  DCHECK(NULL != buffer_handle) << "NULL buffer handle";
  DCHECK(NULL != *buffer_handle) << "NULL pointer";
  DCHECK(NULL != buffer_len_remaining)
      << "NULL buffer remainder length pointer";
  DCHECK_GE(*buffer_len_remaining, len) << "Insufficient buffer size";
  for (int i = 0; i < len; i++) {
    int shift = (8 * (len - (i + 1)));
    unsigned char val_chunk = (val >> shift) & 0x0FF;
    *(*buffer_handle)++ = val_chunk;
    *buffer_len_remaining += 1;
  }
  return len;
}

// Construct a SPDY packet.
// |head| is the start of the packet, up to but not including
// the header value pairs.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// |tail| is any (relatively constant) header-value pairs to add.
// |buffer| is the buffer we're filling in.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyPacket(const SpdyHeaderInfo& header_info,
                               const char* const extra_headers[],
                               int extra_header_count,
                               const char* const tail[],
                               int tail_header_count) {
  BufferedSpdyFramer framer(3, header_info.compressed);
  SpdyHeaderBlock headers;
  // Copy in the extra headers to our map.
  AppendHeadersToSpdyFrame(extra_headers, extra_header_count, &headers);
  // Copy in the tail headers to our map.
  if (tail && tail_header_count)
    AppendHeadersToSpdyFrame(tail, tail_header_count, &headers);
  SpdyFrame* frame = NULL;
  switch (header_info.kind) {
    case SYN_STREAM:
      frame = framer.CreateSynStream(header_info.id, header_info.assoc_id,
                                     header_info.priority,
                                     header_info.credential_slot,
                                     header_info.control_flags,
                                     header_info.compressed, &headers);
      break;
    case SYN_REPLY:
      frame = framer.CreateSynReply(header_info.id, header_info.control_flags,
                                    header_info.compressed, &headers);
      break;
    case RST_STREAM:
      frame = framer.CreateRstStream(header_info.id, header_info.status);
      break;
    case HEADERS:
      frame = framer.CreateHeaders(header_info.id, header_info.control_flags,
                                   header_info.compressed, &headers);
      break;
    default:
      frame = framer.CreateDataFrame(header_info.id, header_info.data,
                                     header_info.data_length,
                                     header_info.data_flags);
      break;
  }
  return frame;
}

// Construct an expected SPDY SETTINGS frame.
// |settings| are the settings to set.
// Returns the constructed frame.  The caller takes ownership of the frame.
SpdyFrame* ConstructSpdySettings(const SettingsMap& settings) {
  BufferedSpdyFramer framer(3, false);
  return framer.CreateSettings(settings);
}

// Construct an expected SPDY CREDENTIAL frame.
// |credential| is the credential to sen.
// Returns the constructed frame.  The caller takes ownership of the frame.
SpdyFrame* ConstructSpdyCredential(
    const SpdyCredential& credential) {
  BufferedSpdyFramer framer(3, false);
  return framer.CreateCredentialFrame(credential);
}

// Construct a SPDY PING frame.
// Returns the constructed frame.  The caller takes ownership of the frame.
SpdyFrame* ConstructSpdyPing(uint32 ping_id) {
  BufferedSpdyFramer framer(3, false);
  return framer.CreatePingFrame(ping_id);
}

// Construct a SPDY GOAWAY frame.
// Returns the constructed frame.  The caller takes ownership of the frame.
SpdyFrame* ConstructSpdyGoAway() {
  BufferedSpdyFramer framer(3, false);
  return framer.CreateGoAway(0, GOAWAY_OK);
}

// Construct a SPDY WINDOW_UPDATE frame.
// Returns the constructed frame.  The caller takes ownership of the frame.
SpdyFrame* ConstructSpdyWindowUpdate(
    const SpdyStreamId stream_id, uint32 delta_window_size) {
  BufferedSpdyFramer framer(3, false);
  return framer.CreateWindowUpdate(stream_id, delta_window_size);
}

// Construct a SPDY RST_STREAM frame.
// Returns the constructed frame.  The caller takes ownership of the frame.
SpdyFrame* ConstructSpdyRstStream(SpdyStreamId stream_id,
                                  SpdyStatusCodes status) {
  BufferedSpdyFramer framer(3, false);
  return framer.CreateRstStream(stream_id, status);
}

// Construct a single SPDY header entry, for validation.
// |extra_headers| are the extra header-value pairs.
// |buffer| is the buffer we're filling in.
// |index| is the index of the header we want.
// Returns the number of bytes written into |buffer|.
int ConstructSpdyHeader(const char* const extra_headers[],
                        int extra_header_count,
                        char* buffer,
                        int buffer_length,
                        int index) {
  const char* this_header = NULL;
  const char* this_value = NULL;
  if (!buffer || !buffer_length)
    return 0;
  *buffer = '\0';
  // Sanity check: Non-empty header list.
  DCHECK(NULL != extra_headers) << "NULL extra headers pointer";
  // Sanity check: Index out of range.
  DCHECK((index >= 0) && (index < extra_header_count))
      << "Index " << index
      << " out of range [0, " << extra_header_count << ")";
  this_header = extra_headers[index * 2];
  // Sanity check: Non-empty header.
  if (!*this_header)
    return 0;
  std::string::size_type header_len = strlen(this_header);
  if (!header_len)
    return 0;
  this_value = extra_headers[1 + (index * 2)];
  // Sanity check: Non-empty value.
  if (!*this_value)
    this_value = "";
  int n = base::snprintf(buffer,
                         buffer_length,
                         "%s: %s\r\n",
                         this_header,
                         this_value);
  return n;
}

SpdyFrame* ConstructSpdyControlFrame(const char* const extra_headers[],
                                     int extra_header_count,
                                     bool compressed,
                                     int stream_id,
                                     RequestPriority request_priority,
                                     SpdyControlType type,
                                     SpdyControlFlags flags,
                                     const char* const* kHeaders,
                                     int kHeadersSize) {
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   compressed,
                                   stream_id,
                                   request_priority,
                                   type,
                                   flags,
                                   kHeaders,
                                   kHeadersSize,
                                   0);
}

SpdyFrame* ConstructSpdyControlFrame(const char* const extra_headers[],
                                     int extra_header_count,
                                     bool compressed,
                                     int stream_id,
                                     RequestPriority request_priority,
                                     SpdyControlType type,
                                     SpdyControlFlags flags,
                                     const char* const* kHeaders,
                                     int kHeadersSize,
                                     int associated_stream_id) {
  const SpdyHeaderInfo kSynStartHeader = {
    type,                         // Kind = Syn
    stream_id,                    // Stream ID
    associated_stream_id,         // Associated stream ID
    ConvertRequestPriorityToSpdyPriority(request_priority, 3),
                                  // Priority
    0,                            // Credential Slot
    flags,                        // Control Flags
    compressed,                   // Compressed
    INVALID,                      // Status
    NULL,                         // Data
    0,                            // Length
    DATA_FLAG_NONE                // Data Flags
  };
  return ConstructSpdyPacket(kSynStartHeader,
                             extra_headers,
                             extra_header_count,
                             kHeaders,
                             kHeadersSize / 2);
}

// Constructs a standard SPDY GET SYN packet, optionally compressed
// for the url |url|.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyGet(const char* const url,
                            bool compressed,
                            int stream_id,
                            RequestPriority request_priority) {
  const SpdyHeaderInfo kSynStartHeader = {
    SYN_STREAM,                   // Kind = Syn
    stream_id,                    // Stream ID
    0,                            // Associated stream ID
    ConvertRequestPriorityToSpdyPriority(request_priority, 3),
                                  // Priority
    0,                            // Credential Slot
    CONTROL_FLAG_FIN,             // Control Flags
    compressed,                   // Compressed
    INVALID,                      // Status
    NULL,                         // Data
    0,                            // Length
    DATA_FLAG_NONE                // Data Flags
  };

  std::string scheme, host, path;
  ParseUrl(url, &scheme, &host, &path);
  const char* const headers[] = {
    ":method", "GET",
    ":path", path.c_str(),
    ":host", host.c_str(),
    ":scheme", scheme.c_str(),
    ":version", "HTTP/1.1"
  };
  return ConstructSpdyPacket(
      kSynStartHeader,
      NULL,
      0,
      headers,
      arraysize(headers) / 2);
}

// Constructs a standard SPDY GET SYN packet, optionally compressed.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyGet(const char* const extra_headers[],
                            int extra_header_count,
                            bool compressed,
                            int stream_id,
                            RequestPriority request_priority) {
  return ConstructSpdyGet(extra_headers, extra_header_count, compressed,
                          stream_id, request_priority, true);
}

// Constructs a standard SPDY GET SYN packet, optionally compressed.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyGet(const char* const extra_headers[],
                            int extra_header_count,
                            bool compressed,
                            int stream_id,
                            RequestPriority request_priority,
                            bool direct) {
  const char* const kStandardGetHeaders[] = {
    ":method", "GET",
    ":host", "www.google.com",
    ":scheme", "http",
    ":version", "HTTP/1.1",
    ":path", (direct ? "/" : "/")
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   compressed,
                                   stream_id,
                                   request_priority,
                                   SYN_STREAM,
                                   CONTROL_FLAG_FIN,
                                   kStandardGetHeaders,
                                   arraysize(kStandardGetHeaders));
}

// Constructs a standard SPDY SYN_STREAM frame for a CONNECT request.
SpdyFrame* ConstructSpdyConnect(const char* const extra_headers[],
                                int extra_header_count,
                                int stream_id) {
  const char* const kConnectHeaders[] = {
    ":method", "CONNECT",
    ":path", "www.google.com:443",
    ":host", "www.google.com",
    ":version", "HTTP/1.1",
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   /*compressed*/ false,
                                   stream_id,
                                   LOWEST,
                                   SYN_STREAM,
                                   CONTROL_FLAG_NONE,
                                   kConnectHeaders,
                                   arraysize(kConnectHeaders));
}

// Constructs a standard SPDY push SYN packet.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyPush(const char* const extra_headers[],
                             int extra_header_count,
                             int stream_id,
                             int associated_stream_id) {
  const char* const kStandardPushHeaders[] = {
    "hello", "bye",
    ":status",  "200",
    ":version", "HTTP/1.1"
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   false,
                                   stream_id,
                                   LOWEST,
                                   SYN_STREAM,
                                   CONTROL_FLAG_NONE,
                                   kStandardPushHeaders,
                                   arraysize(kStandardPushHeaders),
                                   associated_stream_id);
}

SpdyFrame* ConstructSpdyPush(const char* const extra_headers[],
                             int extra_header_count,
                             int stream_id,
                             int associated_stream_id,
                             const char* url) {
  std::string scheme, host, path;
  ParseUrl(url, &scheme, &host, &path);
  const char* const headers[] = {
    "hello",    "bye",
    ":status",  "200 OK",
    ":version", "HTTP/1.1",
    ":path",    path.c_str(),
    ":host",    host.c_str(),
    ":scheme",  scheme.c_str(),
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   false,
                                   stream_id,
                                   LOWEST,
                                   SYN_STREAM,
                                   CONTROL_FLAG_NONE,
                                   headers,
                                   arraysize(headers),
                                   associated_stream_id);

}
SpdyFrame* ConstructSpdyPush(const char* const extra_headers[],
                             int extra_header_count,
                             int stream_id,
                             int associated_stream_id,
                             const char* url,
                             const char* status,
                             const char* location) {
  std::string scheme, host, path;
  ParseUrl(url, &scheme, &host, &path);
  const char* const headers[] = {
    "hello",    "bye",
    ":status",  status,
    "location", location,
    ":path",    path.c_str(),
    ":host",    host.c_str(),
    ":scheme",  scheme.c_str(),
    ":version", "HTTP/1.1"
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   false,
                                   stream_id,
                                   LOWEST,
                                   SYN_STREAM,
                                   CONTROL_FLAG_NONE,
                                   headers,
                                   arraysize(headers),
                                   associated_stream_id);
}

SpdyFrame* ConstructSpdyPushHeaders(int stream_id,
                                    const char* const extra_headers[],
                                    int extra_header_count) {
  const char* const kStandardGetHeaders[] = {
    ":status", "200 OK",
    ":version", "HTTP/1.1"
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   false,
                                   stream_id,
                                   LOWEST,
                                   HEADERS,
                                   CONTROL_FLAG_NONE,
                                   kStandardGetHeaders,
                                   arraysize(kStandardGetHeaders));
}

// Constructs a standard SPDY SYN_REPLY packet with the specified status code.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdySynReplyError(const char* const status,
                                      const char* const* const extra_headers,
                                      int extra_header_count,
                                      int stream_id) {
  const char* const kStandardGetHeaders[] = {
    "hello", "bye",
    ":status",  status,
    ":version", "HTTP/1.1"
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   false,
                                   stream_id,
                                   LOWEST,
                                   SYN_REPLY,
                                   CONTROL_FLAG_NONE,
                                   kStandardGetHeaders,
                                   arraysize(kStandardGetHeaders));
}

// Constructs a standard SPDY SYN_REPLY packet to match the SPDY GET.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyGetSynReplyRedirect(int stream_id) {
  static const char* const kExtraHeaders[] = {
    "location", "http://www.foo.com/index.php",
  };
  return ConstructSpdySynReplyError("301 Moved Permanently", kExtraHeaders,
                                    arraysize(kExtraHeaders)/2, stream_id);
}

// Constructs a standard SPDY SYN_REPLY packet with an Internal Server
// Error status code.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdySynReplyError(int stream_id) {
  return ConstructSpdySynReplyError("500 Internal Server Error", NULL, 0, 1);
}




// Constructs a standard SPDY SYN_REPLY packet to match the SPDY GET.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyGetSynReply(const char* const extra_headers[],
                                    int extra_header_count,
                                    int stream_id) {
  static const char* const kStandardGetHeaders[] = {
    "hello", "bye",
    ":status", "200",
    ":version", "HTTP/1.1"
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   false,
                                   stream_id,
                                   LOWEST,
                                   SYN_REPLY,
                                   CONTROL_FLAG_NONE,
                                   kStandardGetHeaders,
                                   arraysize(kStandardGetHeaders));
}

// Constructs a standard SPDY POST SYN packet.
// |content_length| is the size of post data.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyPost(int64 content_length,
                             const char* const extra_headers[],
                             int extra_header_count) {
  std::string length_str = base::Int64ToString(content_length);
  const char* post_headers[] = {
    ":method", "POST",
    ":path", "/",
    ":host", "www.google.com",
    ":scheme", "http",
    ":version", "HTTP/1.1",
    "content-length",  length_str.c_str()
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   false,
                                   1,
                                   LOWEST,
                                   SYN_STREAM,
                                   CONTROL_FLAG_NONE,
                                   post_headers,
                                   arraysize(post_headers));
}

// Constructs a chunked transfer SPDY POST SYN packet.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
SpdyFrame* ConstructChunkedSpdyPost(const char* const extra_headers[],
                                    int extra_header_count) {
  const char* post_headers[] = {
    ":method", "POST",
    ":path", "/",
    ":host", "www.google.com",
    ":scheme", "http",
    ":version", "HTTP/1.1"
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   false,
                                   1,
                                   LOWEST,
                                   SYN_STREAM,
                                   CONTROL_FLAG_NONE,
                                   post_headers,
                                   arraysize(post_headers));
}

// Constructs a standard SPDY SYN_REPLY packet to match the SPDY POST.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdyFrame.
SpdyFrame* ConstructSpdyPostSynReply(const char* const extra_headers[],
                                     int extra_header_count) {
  static const char* const kStandardGetHeaders[] = {
    "hello", "bye",
    ":status", "200",
    "url", "/index.php",
    ":version", "HTTP/1.1"
  };
  return ConstructSpdyControlFrame(extra_headers,
                                   extra_header_count,
                                   false,
                                   1,
                                   LOWEST,
                                   SYN_REPLY,
                                   CONTROL_FLAG_NONE,
                                   kStandardGetHeaders,
                                   arraysize(kStandardGetHeaders));
}

// Constructs a single SPDY data frame with the default contents.
SpdyFrame* ConstructSpdyBodyFrame(int stream_id, bool fin) {
  BufferedSpdyFramer framer(3, false);
  return framer.CreateDataFrame(
      stream_id, kUploadData, kUploadDataSize,
      fin ? DATA_FLAG_FIN : DATA_FLAG_NONE);
}

// Constructs a single SPDY data frame with the given content.
SpdyFrame* ConstructSpdyBodyFrame(int stream_id, const char* data,
                                  uint32 len, bool fin) {
  BufferedSpdyFramer framer(3, false);
  return framer.CreateDataFrame(
      stream_id, data, len, fin ? DATA_FLAG_FIN : DATA_FLAG_NONE);
}

// Wraps |frame| in the payload of a data frame in stream |stream_id|.
SpdyFrame* ConstructWrappedSpdyFrame(const scoped_ptr<SpdyFrame>& frame,
                                     int stream_id) {
  return ConstructSpdyBodyFrame(stream_id, frame->data(),
                                frame->length() + SpdyFrame::kHeaderSize,
                                false);
}

// Construct an expected SPDY reply string.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// |buffer| is the buffer we're filling in.
// Returns the number of bytes written into |buffer|.
int ConstructSpdyReplyString(const char* const extra_headers[],
                             int extra_header_count,
                             char* buffer,
                             int buffer_length) {
  int packet_size = 0;
  char* buffer_write = buffer;
  int buffer_left = buffer_length;
  SpdyHeaderBlock headers;
  if (!buffer || !buffer_length)
    return 0;
  // Copy in the extra headers.
  AppendHeadersToSpdyFrame(extra_headers, extra_header_count, &headers);
  // The iterator gets us the list of header/value pairs in sorted order.
  SpdyHeaderBlock::iterator next = headers.begin();
  SpdyHeaderBlock::iterator last = headers.end();
  for ( ; next != last; ++next) {
    // Write the header.
    int value_len, current_len, offset;
    const char* header_string = next->first.c_str();
    if (header_string && header_string[0] == ':')
      header_string++;
    packet_size += AppendToBuffer(header_string,
                                  strlen(header_string),
                                  &buffer_write,
                                  &buffer_left);
    packet_size += AppendToBuffer(": ",
                                  strlen(": "),
                                  &buffer_write,
                                  &buffer_left);
    // Write the value(s).
    const char* value_string = next->second.c_str();
    // Check if it's split among two or more values.
    value_len = next->second.length();
    current_len = strlen(value_string);
    offset = 0;
    // Handle the first N-1 values.
    while (current_len < value_len) {
      // Finish this line -- write the current value.
      packet_size += AppendToBuffer(value_string + offset,
                                    current_len - offset,
                                    &buffer_write,
                                    &buffer_left);
      packet_size += AppendToBuffer("\n",
                                    strlen("\n"),
                                    &buffer_write,
                                    &buffer_left);
      // Advance to next value.
      offset = current_len + 1;
      current_len += 1 + strlen(value_string + offset);
      // Start another line -- add the header again.
      packet_size += AppendToBuffer(header_string,
                                    next->first.length(),
                                    &buffer_write,
                                    &buffer_left);
      packet_size += AppendToBuffer(": ",
                                    strlen(": "),
                                    &buffer_write,
                                    &buffer_left);
    }
    EXPECT_EQ(value_len, current_len);
    // Copy the last (or only) value.
    packet_size += AppendToBuffer(value_string + offset,
                                  value_len - offset,
                                  &buffer_write,
                                  &buffer_left);
    packet_size += AppendToBuffer("\n",
                                  strlen("\n"),
                                  &buffer_write,
                                  &buffer_left);
  }
  return packet_size;
}

// Create a MockWrite from the given SpdyFrame.
MockWrite CreateMockWrite(const SpdyFrame& req) {
  return MockWrite(
      ASYNC, req.data(), req.length() + SpdyFrame::kHeaderSize);
}

// Create a MockWrite from the given SpdyFrame and sequence number.
MockWrite CreateMockWrite(const SpdyFrame& req, int seq) {
  return CreateMockWrite(req, seq, ASYNC);
}

// Create a MockWrite from the given SpdyFrame and sequence number.
MockWrite CreateMockWrite(const SpdyFrame& req, int seq, IoMode mode) {
  return MockWrite(
      mode, req.data(), req.length() + SpdyFrame::kHeaderSize, seq);
}

// Create a MockRead from the given SpdyFrame.
MockRead CreateMockRead(const SpdyFrame& resp) {
  return MockRead(
      ASYNC, resp.data(), resp.length() + SpdyFrame::kHeaderSize);
}

// Create a MockRead from the given SpdyFrame and sequence number.
MockRead CreateMockRead(const SpdyFrame& resp, int seq) {
  return CreateMockRead(resp, seq, ASYNC);
}

// Create a MockRead from the given SpdyFrame and sequence number.
MockRead CreateMockRead(const SpdyFrame& resp, int seq, IoMode mode) {
  return MockRead(
      mode, resp.data(), resp.length() + SpdyFrame::kHeaderSize, seq);
}

// Combines the given SpdyFrames into the given char array and returns
// the total length.
int CombineFrames(const SpdyFrame** frames, int num_frames,
                  char* buff, int buff_len) {
  int total_len = 0;
  for (int i = 0; i < num_frames; ++i) {
    total_len += frames[i]->length() + SpdyFrame::kHeaderSize;
  }
  DCHECK_LE(total_len, buff_len);
  char* ptr = buff;
  for (int i = 0; i < num_frames; ++i) {
    int len = frames[i]->length() + SpdyFrame::kHeaderSize;
    memcpy(ptr, frames[i]->data(), len);
    ptr += len;
  }
  return total_len;
}

SpdySessionDependencies::SpdySessionDependencies()
    : host_resolver(new MockCachingHostResolver),
      cert_verifier(new MockCertVerifier),
      proxy_service(ProxyService::CreateDirect()),
      ssl_config_service(new SSLConfigServiceDefaults),
      socket_factory(new MockClientSocketFactory),
      deterministic_socket_factory(new DeterministicMockClientSocketFactory),
      http_auth_handler_factory(
          HttpAuthHandlerFactory::CreateDefault(host_resolver.get())),
      enable_ip_pooling(true),
      enable_compression(false),
      enable_ping(false),
      initial_recv_window_size(kSpdyStreamInitialWindowSize),
      time_func(&base::TimeTicks::Now),
      net_log(NULL) {
  // Note: The CancelledTransaction test does cleanup by running all
  // tasks in the message loop (RunAllPending).  Unfortunately, that
  // doesn't clean up tasks on the host resolver thread; and
  // TCPConnectJob is currently not cancellable.  Using synchronous
  // lookups allows the test to shutdown cleanly.  Until we have
  // cancellable TCPConnectJobs, use synchronous lookups.
  host_resolver->set_synchronous_mode(true);
}

SpdySessionDependencies::SpdySessionDependencies(ProxyService* proxy_service)
    : host_resolver(new MockHostResolver),
      cert_verifier(new MockCertVerifier),
      proxy_service(proxy_service),
      ssl_config_service(new SSLConfigServiceDefaults),
      socket_factory(new MockClientSocketFactory),
      deterministic_socket_factory(new DeterministicMockClientSocketFactory),
      http_auth_handler_factory(
          HttpAuthHandlerFactory::CreateDefault(host_resolver.get())),
      enable_ip_pooling(true),
      enable_compression(false),
      enable_ping(false),
      initial_recv_window_size(kSpdyStreamInitialWindowSize),
      time_func(&base::TimeTicks::Now),
      net_log(NULL) {}

SpdySessionDependencies::~SpdySessionDependencies() {}

// static
HttpNetworkSession* SpdySessionDependencies::SpdyCreateSession(
    SpdySessionDependencies* session_deps) {
  net::HttpNetworkSession::Params params = CreateSessionParams(session_deps);
  params.client_socket_factory = session_deps->socket_factory.get();
  HttpNetworkSession* http_session = new HttpNetworkSession(params);
  SpdySessionPoolPeer pool_peer(http_session->spdy_session_pool());
  pool_peer.EnableSendingInitialSettings(false);
  return http_session;
}

// static
HttpNetworkSession* SpdySessionDependencies::SpdyCreateSessionDeterministic(
    SpdySessionDependencies* session_deps) {
  net::HttpNetworkSession::Params params = CreateSessionParams(session_deps);
  params.client_socket_factory =
      session_deps->deterministic_socket_factory.get();
  HttpNetworkSession* http_session = new HttpNetworkSession(params);
  SpdySessionPoolPeer pool_peer(http_session->spdy_session_pool());
  pool_peer.EnableSendingInitialSettings(false);
  return http_session;
}

// static
net::HttpNetworkSession::Params SpdySessionDependencies::CreateSessionParams(
    SpdySessionDependencies* session_deps) {
  net::HttpNetworkSession::Params params;
  params.host_resolver = session_deps->host_resolver.get();
  params.cert_verifier = session_deps->cert_verifier.get();
  params.proxy_service = session_deps->proxy_service.get();
  params.ssl_config_service = session_deps->ssl_config_service;
  params.http_auth_handler_factory =
      session_deps->http_auth_handler_factory.get();
  params.http_server_properties = &session_deps->http_server_properties;
  params.enable_spdy_compression = session_deps->enable_compression;
  params.enable_spdy_ping_based_connection_checking = session_deps->enable_ping;
  params.spdy_default_protocol = kProtoSPDY3;
  params.spdy_initial_recv_window_size = session_deps->initial_recv_window_size;
  params.time_func = session_deps->time_func;
  params.trusted_spdy_proxy = session_deps->trusted_spdy_proxy;
  params.net_log = session_deps->net_log;
  return params;
}

SpdyURLRequestContext::SpdyURLRequestContext()
    : ALLOW_THIS_IN_INITIALIZER_LIST(storage_(this)) {
  storage_.set_host_resolver(scoped_ptr<HostResolver>(new MockHostResolver));
  storage_.set_cert_verifier(new MockCertVerifier);
  storage_.set_proxy_service(ProxyService::CreateDirect());
  storage_.set_ssl_config_service(new SSLConfigServiceDefaults);
  storage_.set_http_auth_handler_factory(HttpAuthHandlerFactory::CreateDefault(
      host_resolver()));
  storage_.set_http_server_properties(new HttpServerPropertiesImpl);
  net::HttpNetworkSession::Params params;
  params.client_socket_factory = &socket_factory_;
  params.host_resolver = host_resolver();
  params.cert_verifier = cert_verifier();
  params.proxy_service = proxy_service();
  params.ssl_config_service = ssl_config_service();
  params.http_auth_handler_factory = http_auth_handler_factory();
  params.network_delegate = network_delegate();
  params.enable_spdy_compression = false;
  params.enable_spdy_ping_based_connection_checking = false;
  params.spdy_default_protocol = kProtoSPDY3;
  params.http_server_properties = http_server_properties();
  scoped_refptr<HttpNetworkSession> network_session(
      new HttpNetworkSession(params));
  SpdySessionPoolPeer pool_peer(network_session->spdy_session_pool());
  pool_peer.EnableSendingInitialSettings(false);
  storage_.set_http_transaction_factory(new HttpCache(
      network_session,
      HttpCache::DefaultBackend::InMemory(0)));
}

SpdyURLRequestContext::~SpdyURLRequestContext() {
}

const SpdyHeaderInfo MakeSpdyHeader(SpdyControlType type) {
  const SpdyHeaderInfo kHeader = {
    type,                         // Kind = Syn
    1,                            // Stream ID
    0,                            // Associated stream ID
    ConvertRequestPriorityToSpdyPriority(LOWEST, 3),  // Priority
    0,                            // Credential Slot
    CONTROL_FLAG_FIN,       // Control Flags
    false,                        // Compressed
    INVALID,                // Status
    NULL,                         // Data
    0,                            // Length
    DATA_FLAG_NONE          // Data Flags
  };
  return kHeader;
}

}  // namespace test_spdy3

}  // namespace net
