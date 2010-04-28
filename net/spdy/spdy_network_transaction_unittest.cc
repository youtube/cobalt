// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_network_transaction.h"

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/string_util.h"
#include "net/base/completion_callback.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_log_unittest.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/base/test_completion_callback.h"
#include "net/base/upload_data.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_unittest.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_test_util.h"
#include "testing/platform_test.h"

#define NET_TRACE(level, s)   DLOG(level) << s << __FUNCTION__ << "() "

//-----------------------------------------------------------------------------

namespace net {

// NOTE: In GCC, on a Mac, this can't be in an anonymous namespace!
// This struct holds information used to construct spdy control and data frames.
struct SpdyHeaderInfo {
  int kind;
  spdy::SpdyStreamId id;
  spdy::SpdyStreamId assoc_id;
  int priority;
  spdy::SpdyControlFlags control_flags;
  bool compressed;
  int status;
  const char* data;
  uint32 data_length;
  spdy::SpdyDataFlags data_flags;
};

namespace {

// Helper to manage the lifetimes of the dependencies for a
// SpdyNetworkTransaction.
class SessionDependencies {
 public:
  // Default set of dependencies -- "null" proxy service.
  SessionDependencies()
      : host_resolver(new MockHostResolver),
        proxy_service(ProxyService::CreateNull()),
        ssl_config_service(new SSLConfigServiceDefaults),
        http_auth_handler_factory(HttpAuthHandlerFactory::CreateDefault()),
        spdy_session_pool(new SpdySessionPool) {
    // Note: The CancelledTransaction test does cleanup by running all tasks
    // in the message loop (RunAllPending).  Unfortunately, that doesn't clean
    // up tasks on the host resolver thread; and TCPConnectJob is currently
    // not cancellable.  Using synchronous lookups allows the test to shutdown
    // cleanly.  Until we have cancellable TCPConnectJobs, use synchronous
    // lookups.
    host_resolver->set_synchronous_mode(true);
  }

  // Custom proxy service dependency.
  explicit SessionDependencies(ProxyService* proxy_service)
      : host_resolver(new MockHostResolver),
        proxy_service(proxy_service),
        ssl_config_service(new SSLConfigServiceDefaults),
        http_auth_handler_factory(HttpAuthHandlerFactory::CreateDefault()),
        spdy_session_pool(new SpdySessionPool) {}

  scoped_refptr<MockHostResolverBase> host_resolver;
  scoped_refptr<ProxyService> proxy_service;
  scoped_refptr<SSLConfigService> ssl_config_service;
  MockClientSocketFactory socket_factory;
  scoped_ptr<HttpAuthHandlerFactory> http_auth_handler_factory;
  scoped_refptr<SpdySessionPool> spdy_session_pool;
};

HttpNetworkSession* CreateSession(SessionDependencies* session_deps) {
  return new HttpNetworkSession(NULL,
                                session_deps->host_resolver,
                                session_deps->proxy_service,
                                &session_deps->socket_factory,
                                session_deps->ssl_config_service,
                                session_deps->spdy_session_pool,
                                session_deps->http_auth_handler_factory.get());
}

// Chop a frame into an array of MockWrites.
// |data| is the frame to chop.
// |length| is the length of the frame to chop.
// |num_chunks| is the number of chunks to create.
MockWrite* ChopFrame(const char* data, int length, int num_chunks) {
  MockWrite* chunks = new MockWrite[num_chunks];
  int chunk_size = length / num_chunks;
  for (int index = 0; index < num_chunks; index++) {
    const char* ptr = data + (index * chunk_size);
    if (index == num_chunks - 1)
      chunk_size += length % chunk_size;  // The last chunk takes the remainder.
    chunks[index] = MockWrite(true, ptr, chunk_size);
  }
  return chunks;
}

inline char AsciifyHigh(char x) {
  char nybble = static_cast<char>((x >> 4) & 0x0F);
  return nybble + ((nybble < 0x0A) ? '0' : 'A' - 10);
}

inline char AsciifyLow(char x) {
  char nybble = static_cast<char>((x >> 0) & 0x0F);
  return nybble + ((nybble < 0x0A) ? '0' : 'A' - 10);
}

inline char Asciify(char x) {
  if ((x < 0) || !isprint(x))
    return '.';
  return x;
}

void DumpData(const char* data, int data_len) {
  if (logging::LOG_INFO < logging::GetMinLogLevel())
    return;
  DLOG(INFO) << "Length:  " << data_len;
  const char* pfx = "Data:    ";
  if (!data || (data_len <= 0)) {
    DLOG(INFO) << pfx << "<None>";
  } else {
    int i;
    for (i = 0; i <= (data_len - 4); i += 4) {
      DLOG(INFO) << pfx
                 << AsciifyHigh(data[i + 0]) << AsciifyLow(data[i + 0])
                 << AsciifyHigh(data[i + 1]) << AsciifyLow(data[i + 1])
                 << AsciifyHigh(data[i + 2]) << AsciifyLow(data[i + 2])
                 << AsciifyHigh(data[i + 3]) << AsciifyLow(data[i + 3])
                 << "  '"
                 << Asciify(data[i + 0])
                 << Asciify(data[i + 1])
                 << Asciify(data[i + 2])
                 << Asciify(data[i + 3])
                 << "'";
      pfx = "         ";
    }
    // Take care of any 'trailing' bytes, if data_len was not a multiple of 4.
    switch (data_len - i) {
      case 3:
        DLOG(INFO) << pfx
                   << AsciifyHigh(data[i + 0]) << AsciifyLow(data[i + 0])
                   << AsciifyHigh(data[i + 1]) << AsciifyLow(data[i + 1])
                   << AsciifyHigh(data[i + 2]) << AsciifyLow(data[i + 2])
                   << "    '"
                   << Asciify(data[i + 0])
                   << Asciify(data[i + 1])
                   << Asciify(data[i + 2])
                   << " '";
        break;
      case 2:
        DLOG(INFO) << pfx
                   << AsciifyHigh(data[i + 0]) << AsciifyLow(data[i + 0])
                   << AsciifyHigh(data[i + 1]) << AsciifyLow(data[i + 1])
                   << "      '"
                   << Asciify(data[i + 0])
                   << Asciify(data[i + 1])
                   << "  '";
        break;
      case 1:
        DLOG(INFO) << pfx
                   << AsciifyHigh(data[i + 0]) << AsciifyLow(data[i + 0])
                   << "        '"
                   << Asciify(data[i + 0])
                   << "   '";
        break;
    }
  }
}

void DumpMockRead(const MockRead& r) {
  if (logging::LOG_INFO < logging::GetMinLogLevel())
    return;
  DLOG(INFO) << "Async:   " << r.async;
  DLOG(INFO) << "Result:  " << r.result;
  DumpData(r.data, r.data_len);
  const char* stop = (r.sequence_number & MockRead::STOPLOOP) ? " (STOP)" : "";
  DLOG(INFO) << "Stage:   " << (r.sequence_number & ~MockRead::STOPLOOP)
             << stop;
  DLOG(INFO) << "Time:    " << r.time_stamp.ToInternalValue();
}

// ----------------------------------------------------------------------------

// Adds headers and values to a map.
// |extra_headers| is an array of { name, value } pairs, arranged as strings
// where the even entries are the header names, and the odd entries are the
// header values.
// |headers| gets filled in from |extra_headers|.
void AppendHeadersToSpdyFrame(const char* const extra_headers[],
                              int extra_header_count,
                              spdy::SpdyHeaderBlock* headers) {
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

// Writes |str| of the given |len| to the buffer pointed to by |buffer_handle|.
// Uses a template so buffer_handle can be a char* or an unsigned char*.
// Updates the |*buffer_handle| pointer by |len|
// Returns the number of bytes written into *|buffer_handle|
template<class T>
int AppendToBuffer(const void* str,
                   int len,
                   T** buffer_handle,
                   int* buffer_len_remaining) {
  if (len <= 0)
    return 0;
  DCHECK(NULL != buffer_handle) << "NULL buffer handle";
  DCHECK(NULL != *buffer_handle) << "NULL pointer";
  DCHECK(NULL != buffer_len_remaining)
      << "NULL buffer remainder length pointer";
  DCHECK_GE(*buffer_len_remaining, len) << "Insufficient buffer size";
  memcpy(*buffer_handle, str, len);
  *buffer_handle += len;
  *buffer_len_remaining -= len;
  return len;
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
// Returns a SpdFrame.
spdy::SpdyFrame* ConstructSpdyPacket(const SpdyHeaderInfo* header_info,
                                     const char* const extra_headers[],
                                     int extra_header_count,
                                     const char* const tail[],
                                     int tail_header_count) {
  spdy::SpdyFramer framer;
  spdy::SpdyHeaderBlock headers;
  // Copy in the extra headers to our map.
  AppendHeadersToSpdyFrame(extra_headers, extra_header_count, &headers);
  // Copy in the tail headers to our map.
  if (tail && tail_header_count)
    AppendHeadersToSpdyFrame(tail, tail_header_count, &headers);
  spdy::SpdyFrame* frame = NULL;
  switch (header_info->kind) {
    case spdy::SYN_STREAM:
      frame = framer.CreateSynStream(header_info->id, header_info->assoc_id,
                                     header_info->priority,
                                     header_info->control_flags,
                                     header_info->compressed, &headers);
      break;
    case spdy::SYN_REPLY:
      frame = framer.CreateSynReply(header_info->id, header_info->control_flags,
                                    header_info->compressed, &headers);
      break;
    case spdy::RST_STREAM:
      frame = framer.CreateRstStream(header_info->id, header_info->status);
      break;
    default:
      frame = framer.CreateDataFrame(header_info->id, header_info->data,
                                     header_info->data_length,
                                     header_info->data_flags);
      break;
  }
  return frame;
}

// Construct an expected SPDY reply string.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// |buffer| is the buffer we're filling in.
// Returns the number of bytes written into |buffer|.
int ConstructSpdyReply(const char* const extra_headers[],
                       int extra_header_count,
                       char* buffer,
                       int buffer_length) {
  int packet_size = 0;
  int header_count = 0;
  char* buffer_write = buffer;
  int buffer_left = buffer_length;
  spdy::SpdyHeaderBlock headers;
  if (!buffer || !buffer_length)
    return 0;
  // Copy in the extra headers.
  AppendHeadersToSpdyFrame(extra_headers, extra_header_count, &headers);
  header_count = headers.size();
  // The iterator gets us the list of header/value pairs in sorted order.
  spdy::SpdyHeaderBlock::iterator next = headers.begin();
  spdy::SpdyHeaderBlock::iterator last = headers.end();
  for ( ; next != last; ++next) {
    // Write the header.
    int value_len, current_len, offset;
    const char* header_string = next->first.c_str();
    packet_size += AppendToBuffer(header_string,
                                  next->first.length(),
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

// Construct an expected SPDY SETTINGS frame.
// |settings| are the settings to set.
// Returns the constructed frame.  The caller takes ownership of the frame.
spdy::SpdyFrame* ConstructSpdySettings(spdy::SpdySettings settings) {
  spdy::SpdyFramer framer;
  return framer.CreateSettings(settings);
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

// Constructs a standard SPDY GET packet.
// |extra_headers| are the extra header-value pairs, which typically
// will vary the most between calls.
// Returns a SpdFrame.
spdy::SpdyFrame* ConstructSpdyGet(const char* const extra_headers[],
                                  int extra_header_count) {
  SpdyHeaderInfo SynStartHeader = {
    spdy::SYN_STREAM,             // Kind = Syn
    1,                            // Stream ID
    0,                            // Associated stream ID
    3,                            // Priority
    spdy::CONTROL_FLAG_FIN,       // Control Flags
    false,                        // Compressed
    200,                          // Status
    NULL,                         // Data
    0,                            // Length
    spdy::DATA_FLAG_NONE          // Data Flags
  };
  static const char* const kStandardGetHeaders[] = {
    "method",
    "GET",
    "url",
    "http://www.google.com/",
    "version",
    "HTTP/1.1"
  };
  return ConstructSpdyPacket(
      &SynStartHeader,
      extra_headers,
      extra_header_count,
      kStandardGetHeaders,
      arraysize(kStandardGetHeaders) / 2);
}

}  // namespace

// A DataProvider where the reads are ordered.
// If a read is requested before its sequence number is reached, we return an
// ERR_IO_PENDING (that way we don't have to explicitly add a MockRead just to
// wait).
// The sequence number is incremented on every read and write operation.
// The message loop may be interrupted by setting the high bit of the sequence
// number in the MockRead's sequence number.  When that MockRead is reached,
// we post a Quit message to the loop.  This allows us to interrupt the reading
// of data before a complete message has arrived, and provides support for
// testing server push when the request is issued while the response is in the
// middle of being received.
class OrderedSocketData : public StaticSocketDataProvider,
                          public base::RefCounted<OrderedSocketData> {
 public:
  // |reads| the list of MockRead completions.
  // |writes| the list of MockWrite completions.
  // Note: All MockReads and MockWrites must be async.
  // Note: The MockRead and MockWrite lists musts end with a EOF
  //       e.g. a MockRead(true, 0, 0);
  OrderedSocketData(MockRead* reads, size_t reads_count,
                    MockWrite* writes, size_t writes_count)
      : StaticSocketDataProvider(reads, reads_count, writes, writes_count),
        sequence_number_(0), loop_stop_stage_(0), callback_(NULL),
        ALLOW_THIS_IN_INITIALIZER_LIST(factory_(this)) {
  }

  // |connect| the result for the connect phase.
  // |reads| the list of MockRead completions.
  // |writes| the list of MockWrite completions.
  // Note: All MockReads and MockWrites must be async.
  // Note: The MockRead and MockWrite lists musts end with a EOF
  //       e.g. a MockRead(true, 0, 0);
  OrderedSocketData(const MockConnect& connect,
                    MockRead* reads, size_t reads_count,
                    MockWrite* writes, size_t writes_count)
      : StaticSocketDataProvider(reads, reads_count, writes, writes_count),
        sequence_number_(0), loop_stop_stage_(0), callback_(NULL),
        ALLOW_THIS_IN_INITIALIZER_LIST(factory_(this)) {
    set_connect_data(connect);
  }

  virtual MockRead GetNextRead() {
    const MockRead& next_read = StaticSocketDataProvider::PeekRead();
    if (next_read.sequence_number & MockRead::STOPLOOP)
      EndLoop();
    if ((next_read.sequence_number & ~MockRead::STOPLOOP) <=
        sequence_number_++) {
      NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_ - 1
          << ": Read " << read_index();
      DumpMockRead(next_read);
      return StaticSocketDataProvider::GetNextRead();
    }
    NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_ - 1
        << ": I/O Pending";
    MockRead result = MockRead(true, ERR_IO_PENDING);
    DumpMockRead(result);
    return result;
  }

  virtual MockWriteResult OnWrite(const std::string& data) {
    NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_
        << ": Write " << write_index();
    DumpMockRead(PeekWrite());
    ++sequence_number_;
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
      factory_.NewRunnableMethod(&OrderedSocketData::CompleteRead), 100);
    return StaticSocketDataProvider::OnWrite(data);
  }

  virtual void Reset() {
    NET_TRACE(INFO, "  *** ") << "Stage "
        << sequence_number_ << ": Reset()";
    sequence_number_ = 0;
    loop_stop_stage_ = 0;
    set_socket(NULL);
    factory_.RevokeAll();
    StaticSocketDataProvider::Reset();
  }

  void SetCompletionCallback(CompletionCallback* callback) {
    callback_ = callback;
  }

  // Posts a quit message to the current message loop, if one is running.
  void EndLoop() {
    // If we've already stopped the loop, don't do it again until we've advanced
    // to the next sequence_number.
    NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_ << ": EndLoop()";
    if (loop_stop_stage_ > 0) {
      const MockRead& next_read = StaticSocketDataProvider::PeekRead();
      if ((next_read.sequence_number & ~MockRead::STOPLOOP) >
          loop_stop_stage_) {
        NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_
            << ": Clearing stop index";
        loop_stop_stage_ = 0;
      } else {
        return;
      }
    }
    // Record the sequence_number at which we stopped the loop.
    NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_
        << ": Posting Quit at read " << read_index();
    loop_stop_stage_ = sequence_number_;
    if (callback_)
      callback_->RunWithParams(Tuple1<int>(ERR_IO_PENDING));
  }

  void CompleteRead() {
    if (socket()) {
      NET_TRACE(INFO, "  *** ") << "Stage " << sequence_number_;
      socket()->OnReadComplete(GetNextRead());
    }
  }

 private:
  int sequence_number_;
  int loop_stop_stage_;
  CompletionCallback* callback_;
  ScopedRunnableMethodFactory<OrderedSocketData> factory_;
};

class SpdyNetworkTransactionTest : public PlatformTest {
 protected:
  virtual void SetUp() {
    // By default, all tests turn off compression.
    EnableCompression(false);
  }

  virtual void TearDown() {
    // Empty the current queue.
    MessageLoop::current()->RunAllPending();
    PlatformTest::TearDown();
  }

  void KeepAliveConnectionResendRequestTest(const MockRead& read_failure);

  struct TransactionHelperResult {
    int rv;
    std::string status_line;
    std::string response_data;
    HttpResponseInfo response_info;
  };

  void EnableCompression(bool enabled) {
    spdy::SpdyFramer::set_enable_compression_default(enabled);
  }

  TransactionHelperResult TransactionHelper(const HttpRequestInfo& request,
                                            DelayedSocketData* data,
                                            const BoundNetLog& log) {
    SessionDependencies session_deps;
    HttpNetworkSession* session = CreateSession(&session_deps);
    return TransactionHelperWithSession(request, data, log, &session_deps,
                                        session);
  }

  TransactionHelperResult TransactionHelperWithSession(
      const HttpRequestInfo& request, DelayedSocketData* data,
      const BoundNetLog& log, SessionDependencies* session_deps,
      HttpNetworkSession* session) {
    CHECK(session);
    CHECK(session_deps);

    TransactionHelperResult out;

    // We disable SSL for this test.
    SpdySession::SetSSLMode(false);

    scoped_ptr<SpdyNetworkTransaction> trans(
        new SpdyNetworkTransaction(session));

    session_deps->socket_factory.AddSocketDataProvider(data);

    TestCompletionCallback callback;

    out.rv = trans->Start(&request, &callback, log);
    EXPECT_LT(out.rv, 0);  // We expect an IO Pending or some sort of error.
    if (out.rv != ERR_IO_PENDING)
      return out;

    out.rv = callback.WaitForResult();
    if (out.rv != OK)
      return out;

    const HttpResponseInfo* response = trans->GetResponseInfo();
    EXPECT_TRUE(response->headers != NULL);
    EXPECT_TRUE(response->was_fetched_via_spdy);
    out.status_line = response->headers->GetStatusLine();
    out.response_info = *response;  // Make a copy so we can verify.

    out.rv = ReadTransaction(trans.get(), &out.response_data);
    EXPECT_EQ(OK, out.rv);

    // Verify that we consumed all test data.
    EXPECT_TRUE(data->at_read_eof());
    EXPECT_TRUE(data->at_write_eof());

    return out;
  }

  void ConnectStatusHelperWithExpectedStatus(const MockRead& status,
                                             int expected_status);

  void ConnectStatusHelper(const MockRead& status);
};

//-----------------------------------------------------------------------------

// Verify SpdyNetworkTransaction constructor.
TEST_F(SpdyNetworkTransactionTest, Constructor) {
  SessionDependencies session_deps;
  scoped_refptr<HttpNetworkSession> session =
      CreateSession(&session_deps);
  scoped_ptr<HttpTransaction> trans(new SpdyNetworkTransaction(session));
}

TEST_F(SpdyNetworkTransactionTest, Get) {
  MockWrite writes[] = {
    MockWrite(true, reinterpret_cast<const char*>(kGetSyn),
              arraysize(kGetSyn)),
  };

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(kGetSynReply),
             arraysize(kGetSynReply)),
    MockRead(true, reinterpret_cast<const char*>(kGetBodyFrame),
             arraysize(kGetBodyFrame)),
    MockRead(true, 0, 0)  // EOF
  };

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;
  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  TransactionHelperResult out = TransactionHelper(request, data.get(),
                                                  BoundNetLog());
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// Test that a simple POST works.
TEST_F(SpdyNetworkTransactionTest, Post) {
  static const char upload[] = { "hello world" };

  // Setup the request
  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  request.upload_data = new UploadData();
  request.upload_data->AppendBytes(upload, sizeof(upload));

  MockWrite writes[] = {
    MockWrite(true, reinterpret_cast<const char*>(kPostSyn),
              arraysize(kPostSyn)),
    MockWrite(true, reinterpret_cast<const char*>(kPostUploadFrame),
              arraysize(kPostUploadFrame)),
  };

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(kPostSynReply),
             arraysize(kPostSynReply)),
    MockRead(true, reinterpret_cast<const char*>(kPostBodyFrame),
             arraysize(kPostBodyFrame)),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(2, reads, arraysize(reads),
                            writes, arraysize(writes)));
  TransactionHelperResult out = TransactionHelper(request, data.get(),
                                                  BoundNetLog());
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// Test that a simple POST works.
TEST_F(SpdyNetworkTransactionTest, EmptyPost) {
static const unsigned char kEmptyPostSyn[] = {
  0x80, 0x01, 0x00, 0x01,                                      // header
  0x01, 0x00, 0x00, 0x4a,                                      // flags, len
  0x00, 0x00, 0x00, 0x01,                                      // stream id
  0x00, 0x00, 0x00, 0x00,                                      // associated
  0xc0, 0x00, 0x00, 0x03,                                      // 4 headers
  0x00, 0x06, 'm', 'e', 't', 'h', 'o', 'd',
  0x00, 0x04, 'P', 'O', 'S', 'T',
  0x00, 0x03, 'u', 'r', 'l',
  0x00, 0x16, 'h', 't', 't', 'p', ':', '/', '/', 'w', 'w', 'w',
              '.', 'g', 'o', 'o', 'g', 'l', 'e', '.', 'c', 'o',
              'm', '/',
  0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',
  0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',
};

  // Setup the request
  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  // Create an empty UploadData.
  request.upload_data = new UploadData();

  MockWrite writes[] = {
    MockWrite(true, reinterpret_cast<const char*>(kEmptyPostSyn),
              arraysize(kEmptyPostSyn)),
  };

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(kPostSynReply),
             arraysize(kPostSynReply)),
    MockRead(true, reinterpret_cast<const char*>(kPostBodyFrame),
             arraysize(kPostBodyFrame)),
    MockRead(true, 0, 0)  // EOF
  };

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));

  TransactionHelperResult out = TransactionHelper(request, data, BoundNetLog());
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// Test that the transaction doesn't crash when we don't have a reply.
TEST_F(SpdyNetworkTransactionTest, ResponseWithoutSynReply) {
  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(kPostBodyFrame),
             arraysize(kPostBodyFrame)),
    MockRead(true, 0, 0)  // EOF
  };

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;
  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads), NULL, 0));
  TransactionHelperResult out = TransactionHelper(request, data.get(),
                                                  BoundNetLog());
  EXPECT_EQ(ERR_SYN_REPLY_NOT_RECEIVED, out.rv);
}

TEST_F(SpdyNetworkTransactionTest, CancelledTransaction) {
  MockWrite writes[] = {
    MockWrite(true, reinterpret_cast<const char*>(kGetSyn),
              arraysize(kGetSyn)),
    MockRead(true, 0, 0)  // EOF
  };

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(kGetSynReply),
             arraysize(kGetSynReply)),
    // This following read isn't used by the test, except during the
    // RunAllPending() call at the end since the SpdySession survives the
    // SpdyNetworkTransaction and still tries to continue Read()'ing.  Any
    // MockRead will do here.
    MockRead(true, 0, 0)  // EOF
  };

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  // We disable SSL for this test.
  SpdySession::SetSSLMode(false);

  SessionDependencies session_deps;
  scoped_ptr<SpdyNetworkTransaction> trans(
      new SpdyNetworkTransaction(CreateSession(&session_deps)));

  StaticSocketDataProvider data(reads, arraysize(reads),
                                writes, arraysize(writes));
  session_deps.socket_factory.AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  trans.reset();  // Cancel the transaction.

  // Flush the MessageLoop while the SessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  MessageLoop::current()->RunAllPending();
}

// Verify that various SynReply headers parse correctly through the
// HTTP layer.
TEST_F(SpdyNetworkTransactionTest, SynReplyHeaders) {
  // This uses a multi-valued cookie header.
  static const unsigned char syn_reply1[] = {
    0x80, 0x01, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x4c,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x04,
    0x00, 0x06, 'c', 'o', 'o', 'k', 'i', 'e',
    0x00, 0x09, 'v', 'a', 'l', '1', '\0',
                'v', 'a', 'l', '2',
    0x00, 0x06, 's', 't', 'a', 't', 'u', 's',
    0x00, 0x03, '2', '0', '0',
    0x00, 0x03, 'u', 'r', 'l',
    0x00, 0x0a, '/', 'i', 'n', 'd', 'e', 'x', '.', 'p', 'h', 'p',
    0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',
    0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',
  };

  // This is the minimalist set of headers.
  static const unsigned char syn_reply2[] = {
    0x80, 0x01, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x39,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x04,
    0x00, 0x06, 's', 't', 'a', 't', 'u', 's',
    0x00, 0x03, '2', '0', '0',
    0x00, 0x03, 'u', 'r', 'l',
    0x00, 0x0a, '/', 'i', 'n', 'd', 'e', 'x', '.', 'p', 'h', 'p',
    0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',
    0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',
  };

  // Headers with a comma separated list.
  static const unsigned char syn_reply3[] = {
    0x80, 0x01, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x4c,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x04,
    0x00, 0x06, 'c', 'o', 'o', 'k', 'i', 'e',
    0x00, 0x09, 'v', 'a', 'l', '1', ',', 'v', 'a', 'l', '2',
    0x00, 0x06, 's', 't', 'a', 't', 'u', 's',
    0x00, 0x03, '2', '0', '0',
    0x00, 0x03, 'u', 'r', 'l',
    0x00, 0x0a, '/', 'i', 'n', 'd', 'e', 'x', '.', 'p', 'h', 'p',
    0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',
    0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',
  };

  struct SynReplyTests {
    const unsigned char* syn_reply;
    int syn_reply_length;
    const char* expected_headers;
  } test_cases[] = {
    // Test the case of a multi-valued cookie.  When the value is delimited
    // with NUL characters, it needs to be unfolded into multiple headers.
    { syn_reply1, sizeof(syn_reply1),
      "cookie: val1\n"
      "cookie: val2\n"
      "status: 200\n"
      "url: /index.php\n"
      "version: HTTP/1.1\n"
    },
    // This is the simplest set of headers possible.
    { syn_reply2, sizeof(syn_reply2),
      "status: 200\n"
      "url: /index.php\n"
      "version: HTTP/1.1\n"
    },
    // Test that a comma delimited list is NOT interpreted as a multi-value
    // name/value pair.  The comma-separated list is just a single value.
    { syn_reply3, sizeof(syn_reply3),
      "cookie: val1,val2\n"
      "status: 200\n"
      "url: /index.php\n"
      "version: HTTP/1.1\n"
    }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    MockWrite writes[] = {
      MockWrite(true, reinterpret_cast<const char*>(kGetSyn),
                arraysize(kGetSyn)),
    };

    MockRead reads[] = {
      MockRead(true, reinterpret_cast<const char*>(test_cases[i].syn_reply),
               test_cases[i].syn_reply_length),
      MockRead(true, reinterpret_cast<const char*>(kGetBodyFrame),
               arraysize(kGetBodyFrame)),
      MockRead(true, 0, 0)  // EOF
    };

    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/");
    request.load_flags = 0;
    scoped_refptr<DelayedSocketData> data(
        new DelayedSocketData(1, reads, arraysize(reads),
                              writes, arraysize(writes)));
    TransactionHelperResult out = TransactionHelper(request, data.get(),
                                                    BoundNetLog());
    EXPECT_EQ(OK, out.rv);
    EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
    EXPECT_EQ("hello!", out.response_data);

    scoped_refptr<HttpResponseHeaders> headers = out.response_info.headers;
    EXPECT_TRUE(headers.get() != NULL);
    void* iter = NULL;
    std::string name, value, lines;
    while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
      lines.append(name);
      lines.append(": ");
      lines.append(value);
      lines.append("\n");
    }
    EXPECT_EQ(std::string(test_cases[i].expected_headers), lines);
  }
}

// Verify that various SynReply headers parse vary fields correctly
// through the HTTP layer, and the response matches the request.
TEST_F(SpdyNetworkTransactionTest, SynReplyHeadersVary) {
  static const SpdyHeaderInfo syn_reply_info = {
    spdy::SYN_REPLY,                              // Syn Reply
    1,                                            // Stream ID
    0,                                            // Associated Stream ID
    3,                                            // Priority
    spdy::CONTROL_FLAG_NONE,                      // Control Flags
    false,                                        // Compressed
    200,                                          // Status
    NULL,                                         // Data
    0,                                            // Data Length
    spdy::DATA_FLAG_NONE                          // Data Flags
  };
  // Modify the following data to change/add test cases:
  struct SynReplyTests {
    const SpdyHeaderInfo* syn_reply;
    bool vary_matches;
    int num_headers[2];
    const char* extra_headers[2][16];
  } test_cases[] = {
    // Test the case of a multi-valued cookie.  When the value is delimited
    // with NUL characters, it needs to be unfolded into multiple headers.
    {
      &syn_reply_info,
      true,
      { 1, 4 },
      { { "cookie",   "val1,val2",
          NULL
        },
        { "vary",     "cookie",
          "status",   "200",
          "url",      "/index.php",
          "version",  "HTTP/1.1",
          NULL
        }
      }
    }, {    // Multiple vary fields.
      &syn_reply_info,
      true,
      { 2, 5 },
      { { "friend",   "barney",
          "enemy",    "snaggletooth",
          NULL
        },
        { "vary",     "friend",
          "vary",     "enemy",
          "status",   "200",
          "url",      "/index.php",
          "version",  "HTTP/1.1",
          NULL
        }
      }
    }, {    // Test a '*' vary field.
      &syn_reply_info,
      false,
      { 1, 4 },
      { { "cookie",   "val1,val2",
          NULL
        },
        { "vary",     "*",
          "status",   "200",
          "url",      "/index.php",
          "version",  "HTTP/1.1",
          NULL
        }
      }
    }, {    // Multiple comma-separated vary fields.
      &syn_reply_info,
      true,
      { 2, 4 },
      { { "friend",   "barney",
          "enemy",    "snaggletooth",
          NULL
        },
        { "vary",     "friend,enemy",
          "status",   "200",
          "url",      "/index.php",
          "version",  "HTTP/1.1",
          NULL
        }
      }
    }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    // Construct the request.
    scoped_ptr<spdy::SpdyFrame> frame_req(
      ConstructSpdyGet(test_cases[i].extra_headers[0],
                       test_cases[i].num_headers[0]));

    MockWrite writes[] = {
      MockWrite(
          true,
          frame_req->data(),
          frame_req->length() + spdy::SpdyFrame::size()),
    };

    // Construct the reply.
    scoped_ptr<spdy::SpdyFrame> frame_reply(
      ConstructSpdyPacket(test_cases[i].syn_reply,
                          test_cases[i].extra_headers[1],
                          test_cases[i].num_headers[1],
                          NULL,
                          0));

    MockRead reads[] = {
      MockRead(true,
               frame_reply->data(),
               frame_reply->length() + spdy::SpdyFrame::size()),
      MockRead(true,
               reinterpret_cast<const char*>(kGetBodyFrame),
               arraysize(kGetBodyFrame)),
      MockRead(true, 0, 0)  // EOF
    };

    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/");
    request.load_flags = 0;

    // Attach the headers to the request.
    int header_count = test_cases[i].num_headers[0];

    for (int ct = 0; ct < header_count; ct++) {
      const char* header_key = test_cases[i].extra_headers[0][ct * 2];
      const char* header_value = test_cases[i].extra_headers[0][ct * 2 + 1];
      request.extra_headers.SetHeader(header_key, header_value);
    }

    scoped_refptr<DelayedSocketData> data(
        new DelayedSocketData(1, reads, arraysize(reads),
                              writes, arraysize(writes)));
    TransactionHelperResult out = TransactionHelper(request, data.get(),
                                                    BoundNetLog());
    EXPECT_EQ(OK, out.rv) << i;
    EXPECT_EQ("HTTP/1.1 200 OK", out.status_line) << i;
    EXPECT_EQ("hello!", out.response_data) << i;

    // Test the response information.
    EXPECT_TRUE(out.response_info.response_time >
                out.response_info.request_time) << i;
    base::TimeDelta test_delay = out.response_info.response_time -
                                 out.response_info.request_time;
    base::TimeDelta min_expected_delay;
    min_expected_delay.FromMilliseconds(10);
    EXPECT_GT(test_delay.InMillisecondsF(),
              min_expected_delay.InMillisecondsF()) << i;
    EXPECT_EQ(out.response_info.vary_data.is_valid(),
              test_cases[i].vary_matches) << i;

    // Check the headers.
    scoped_refptr<HttpResponseHeaders> headers = out.response_info.headers;
    ASSERT_TRUE(headers.get() != NULL) << i;
    void* iter = NULL;
    std::string name, value, lines;
    while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
      lines.append(name);
      lines.append(": ");
      lines.append(value);
      lines.append("\n");
    }

    // Construct the expected header reply string.
    char reply_buffer[256] = "";
    ConstructSpdyReply(test_cases[i].extra_headers[1],
                       test_cases[i].num_headers[1],
                       reply_buffer,
                       256);

    EXPECT_EQ(std::string(reply_buffer), lines) << i;
  }
}

// Verify that we don't crash on invalid SynReply responses.
TEST_F(SpdyNetworkTransactionTest, InvalidSynReply) {
  static const unsigned char kSynReplyMissingStatus[] = {
    0x80, 0x01, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x3f,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x04,
    0x00, 0x06, 'c', 'o', 'o', 'k', 'i', 'e',
    0x00, 0x09, 'v', 'a', 'l', '1', '\0',
                'v', 'a', 'l', '2',
    0x00, 0x03, 'u', 'r', 'l',
    0x00, 0x0a, '/', 'i', 'n', 'd', 'e', 'x', '.', 'p', 'h', 'p',
    0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',
    0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',
  };

  static const unsigned char kSynReplyMissingVersion[] = {
    0x80, 0x01, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x26,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x04,
    0x00, 0x06, 's', 't', 'a', 't', 'u', 's',
    0x00, 0x03, '2', '0', '0',
    0x00, 0x03, 'u', 'r', 'l',
    0x00, 0x0a, '/', 'i', 'n', 'd', 'e', 'x', '.', 'p', 'h', 'p',
  };

  struct SynReplyTests {
    const unsigned char* syn_reply;
    int syn_reply_length;
  } test_cases[] = {
    { kSynReplyMissingStatus, arraysize(kSynReplyMissingStatus) },
    { kSynReplyMissingVersion, arraysize(kSynReplyMissingVersion) }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    MockWrite writes[] = {
      MockWrite(true, reinterpret_cast<const char*>(kGetSyn),
                arraysize(kGetSyn)),
      MockWrite(true, 0, 0)  // EOF
    };

    MockRead reads[] = {
      MockRead(true, reinterpret_cast<const char*>(test_cases[i].syn_reply),
               test_cases[i].syn_reply_length),
      MockRead(true, reinterpret_cast<const char*>(kGetBodyFrame),
               arraysize(kGetBodyFrame)),
      MockRead(true, 0, 0)  // EOF
    };

    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/");
    request.load_flags = 0;
    scoped_refptr<DelayedSocketData> data(
        new DelayedSocketData(1, reads, arraysize(reads),
                              writes, arraysize(writes)));
    TransactionHelperResult out = TransactionHelper(request, data.get(),
                                                    BoundNetLog());
    EXPECT_EQ(ERR_INVALID_RESPONSE, out.rv);
  }
}

// Verify that we don't crash on some corrupt frames.
TEST_F(SpdyNetworkTransactionTest, CorruptFrameSessionError) {
  static const unsigned char kSynReplyMassiveLength[] = {
    0x80, 0x01, 0x00, 0x02,
    0x0f, 0x11, 0x11, 0x26,   // This is the length field with a big number
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x04,
    0x00, 0x06, 's', 't', 'a', 't', 'u', 's',
    0x00, 0x03, '2', '0', '0',
    0x00, 0x03, 'u', 'r', 'l',
    0x00, 0x0a, '/', 'i', 'n', 'd', 'e', 'x', '.', 'p', 'h', 'p',
  };

  struct SynReplyTests {
    const unsigned char* syn_reply;
    int syn_reply_length;
  } test_cases[] = {
    { kSynReplyMassiveLength, arraysize(kSynReplyMassiveLength) }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    MockWrite writes[] = {
      MockWrite(true, reinterpret_cast<const char*>(kGetSyn),
                arraysize(kGetSyn)),
      MockWrite(true, 0, 0)  // EOF
    };

    MockRead reads[] = {
      MockRead(true, reinterpret_cast<const char*>(test_cases[i].syn_reply),
               test_cases[i].syn_reply_length),
      MockRead(true, reinterpret_cast<const char*>(kGetBodyFrame),
               arraysize(kGetBodyFrame)),
      MockRead(true, 0, 0)  // EOF
    };

    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/");
    request.load_flags = 0;
    scoped_refptr<DelayedSocketData> data(
        new DelayedSocketData(1, reads, arraysize(reads),
                              writes, arraysize(writes)));
    TransactionHelperResult out = TransactionHelper(request, data.get(),
                                                    BoundNetLog());
    EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, out.rv);
  }
}

// Server push:
// ------------
// Client: Send the original SYN request.
// Server: Receive the SYN request.
// Server: Send a SYN reply, with X-Associated-Content and URL(s).
// Server: For each URL, send a SYN_STREAM with the URL and a stream ID,
//         followed by one or more Data frames (the last one with a FIN).
// Client: Requests the URL(s).
// Client: Receives the SYN_STREAMs, and the associated Data frames, and
//         associates the URLs with the incoming stream IDs.
//
// There are three possibilities when the client tries to send the second
// request (which doesn't make it to the wire):
//
// 1. The push data has arrived and is complete.
// 2. The push data has started arriving, but hasn't finished.
// 3. The push data has not yet arrived.

// Enum for ServerPush.
enum TestTypes {
  // Simulate that the server sends the first request, notifying the client
  // that it *will* push the second stream.  But the client issues the
  // request for the second stream before the push data arrives.
  PUSH_AFTER_REQUEST,
  // Simulate that the server is sending the pushed stream data before the
  // client requests it.  The SpdySession will buffer the response and then
  // deliver the data when the client does make the request.
  PUSH_BEFORE_REQUEST,
  // Simulate that the server is sending the pushed stream data before the
  // client requests it, but the stream has not yet finished when the request
  // occurs.  The SpdySession will buffer the response and then deliver the
  // data when the response is complete.
  PUSH_DURING_REQUEST,
  DONE
};

// Creates and processes a SpdyNetworkTransaction for server push, based on
//   |session|.
// |data| holds the expected writes, and the reads.
// |url| is the web page we want.  In pass 2, it contains the resource we expect
//   to be pushed.
// |expected_data| is the data we expect to get in response.
// |test_type| is one of PUSH_AFTER_REQUEST, PUSH_BEFORE_REQUEST, or
//   PUSH_DURING_REQUEST, indicating the type of test we're running.
// |pass| is 1 for the first request, and 2 for the request for the push data.
// |response| is the response information for the request.  It will be used to
//   verify the response time stamps.
static void MakeRequest(scoped_refptr<HttpNetworkSession> session,
                        scoped_refptr<OrderedSocketData> data,
                        const GURL& url,
                        const std::string& expected_data,
                        int test_type,
                        int pass,
                        HttpResponseInfo* response) {
  SpdyNetworkTransaction trans(session.get());

  HttpRequestInfo request;
  request.method = "GET";
  request.url = url;
  request.load_flags = 0;
  TestCompletionCallback callback;

  // Allows the STOP_LOOP flag to work.
  data->SetCompletionCallback(&callback);
  // Sends a request.  In pass 1, this goes on the wire; in pass 2, it is
  // preempted by the push data.
  int rv = trans.Start(&request, &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // In the case where we are pushing beforehand, complete the next read now.
  if ((pass == 2) && (test_type == PUSH_AFTER_REQUEST)) {
    data->CompleteRead();
  }

  // Process messages until either a FIN or a STOP_LOOP is encountered.
  rv = callback.WaitForResult();
  if ((pass == 2) && (test_type == PUSH_DURING_REQUEST)) {
    // We should be in the middle of a request, so we're pending.
    EXPECT_EQ(ERR_IO_PENDING, rv);
  } else {
    EXPECT_EQ(OK, rv);
  }

  // Verify the SYN_REPLY.
  // Copy the response info, because trans goes away
  *response = *trans.GetResponseInfo();
  EXPECT_TRUE(response->headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  // In the case where we are Complete the next read now.
  if (((pass == 1) &&
      ((test_type == PUSH_BEFORE_REQUEST) ||
          (test_type == PUSH_DURING_REQUEST)))) {
    data->CompleteRead();
  }

  // Verify the body.
  std::string response_data;
  rv = ReadTransaction(&trans, &response_data);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ(expected_data, response_data);
  // Remove callback, so that if another STOP_LOOP occurs, there is no crash.
  data->SetCompletionCallback(NULL);
}

TEST_F(SpdyNetworkTransactionTest, ServerPush) {
  // Reply with the X-Associated-Content header.
  static const unsigned char syn_reply[] = {
    0x80, 0x01, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x71,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x04,
    0x00, 0x14, 'X', '-', 'A', 's', 's', 'o', 'c', 'i', 'a', 't',
                'e', 'd', '-', 'C', 'o', 'n', 't', 'e', 'n', 't',
    0x00, 0x20, '1', '?', '?', 'h', 't', 't', 'p', ':', '/', '/', 'w', 'w',
                'w', '.', 'g', 'o', 'o', 'g', 'l', 'e', '.', 'c', 'o', 'm',
                '/', 'f', 'o', 'o', '.', 'd', 'a', 't',
    0x00, 0x06, 's', 't', 'a', 't', 'u', 's',
    0x00, 0x03, '2', '0', '0',
    0x00, 0x03, 'u', 'r', 'l',
    0x00, 0x0a, '/', 'i', 'n', 'd', 'e', 'x', '.', 'p', 'h', 'p',
    0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',
    0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',
  };

  // Syn for the X-Associated-Content (foo.dat)
  static const unsigned char syn_push[] = {
    0x80, 0x01, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x4b,
    0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x00,  // TODO(mbelshe): use new server push protocol.
    0x00, 0x00, 0x00, 0x04,
    0x00, 0x04, 'p', 'a', 't', 'h',
    0x00, 0x08, '/', 'f', 'o', 'o', '.', 'd', 'a', 't',
    0x00, 0x06, 's', 't', 'a', 't', 'u', 's',
    0x00, 0x03, '2', '0', '0',
    0x00, 0x03, 'u', 'r', 'l',
    0x00, 0x08, '/', 'f', 'o', 'o', '.', 'd', 'a', 't',
    0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',
    0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',
  };

  // Body for stream 2
  static const unsigned char kPushBodyFrame[] = {
    0x00, 0x00, 0x00, 0x02,                                      // header, ID
    0x01, 0x00, 0x00, 0x05,                                      // FIN, length
    'h', 'e', 'l', 'l', 'o',                                     // "hello"
  };

  static const unsigned char kPushBodyFrame1[] = {
    0x00, 0x00, 0x00, 0x02,                                      // header, ID
    0x01, 0x00, 0x00, 0x1E,                                      // FIN, length
    'h', 'e', 'l', 'l', 'o',                                     // "hello"
  };

  static const char kPushBodyFrame2[] = " my darling";
  static const char kPushBodyFrame3[] = " hello";
  static const char kPushBodyFrame4[] = " my baby";

  const char syn_body_data1[] = "hello";
  const char syn_body_data2[] = "hello my darling hello my baby";
  const char* syn_body_data = NULL;

  MockWrite writes[] = {
    MockWrite(true, reinterpret_cast<const char*>(kGetSyn),
              arraysize(kGetSyn)),
  };

  // This array is for request before and after push is received.  The push
  // body is only one 'packet', to allow the initial transaction to read all
  // the push data before .
  MockRead reads1[] = {
    MockRead(true, reinterpret_cast<const char*>(syn_reply),        // 0
             arraysize(syn_reply), 2),
    MockRead(true, reinterpret_cast<const char*>(kGetBodyFrame),    // 1
             arraysize(kGetBodyFrame), 3),
    MockRead(true, ERR_IO_PENDING, 4),  // Force a pause            // 2
    MockRead(true, reinterpret_cast<const char*>(syn_push),         // 3
             arraysize(syn_push), 5),
    MockRead(true, reinterpret_cast<const char*>(kPushBodyFrame),   // 4
             arraysize(kPushBodyFrame), 6),
    MockRead(true, ERR_IO_PENDING, 7),  // Force a pause            // 5
    MockRead(true, 0, 0, 8)  // EOF                                 // 6
  };

  // This array is for request while push is being received.  It extends
  // the push body so we can 'interrupt' it.
  MockRead reads2[] = {
    MockRead(true, reinterpret_cast<const char*>(syn_reply),        // 0
             arraysize(syn_reply), 2),
    MockRead(true, reinterpret_cast<const char*>(kGetBodyFrame),    // 1
             arraysize(kGetBodyFrame), 3),
    MockRead(true, reinterpret_cast<const char*>(syn_push),         // 2
             arraysize(syn_push), 4),
    MockRead(true, reinterpret_cast<const char*>(kPushBodyFrame1),  // 3
             arraysize(kPushBodyFrame1), 5),
    // Force a pause by skipping a sequence number.
    MockRead(true, reinterpret_cast<const char*>(kPushBodyFrame2),  // 4
             arraysize(kPushBodyFrame2) - 1, 7),
    MockRead(true, reinterpret_cast<const char*>(kPushBodyFrame3),  // 5
             arraysize(kPushBodyFrame3) - 1, 8),
    MockRead(true, reinterpret_cast<const char*>(kPushBodyFrame4),  // 6
             arraysize(kPushBodyFrame4) - 1, 9),
    MockRead(true, ERR_IO_PENDING, MockRead::STOPLOOP | 10),        // 7
    // So we can do a final CompleteRead(), which cleans up memory.
    MockRead(true, NULL, 0, 11)                                     // 8
  };

  // We disable SSL for this test.
  SpdySession::SetSSLMode(false);

  base::Time zero_time = base::Time::FromInternalValue(0);
  for (int test_type = PUSH_AFTER_REQUEST; test_type < DONE; ++test_type) {
    DLOG(INFO) << "Test " << test_type;

    // Select the data to use.
    MockRead* reads = NULL;
    size_t num_reads = 0;
    size_t num_writes = arraysize(writes);
    int first_push_data_frame = 0;
    if (test_type == PUSH_DURING_REQUEST) {
      reads = reads2;
      num_reads = arraysize(reads2);
      syn_body_data = syn_body_data2;
      first_push_data_frame = 3;
    } else {
      reads = reads1;
      num_reads = arraysize(reads1);
      syn_body_data = syn_body_data1;
      first_push_data_frame = 4;
    }
    // Clear timestamp data
    for (size_t w = 0; w < num_writes; ++w) {
      writes[w].time_stamp = zero_time;
    }
    for (size_t r = 0; r < num_reads; ++r) {
      reads[r].time_stamp = zero_time;
    }

    // Setup a mock session.
    SessionDependencies session_deps;
    scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));
    scoped_refptr<OrderedSocketData> data(
        new OrderedSocketData(reads, num_reads, writes, num_writes));
    session_deps.socket_factory.AddSocketDataProvider(data.get());
    HttpResponseInfo response1;
    HttpResponseInfo response2;

    DLOG(INFO) << "Sending request 1";

    // Issue the first request.
    MakeRequest(session,
                data,
                GURL("http://www.google.com/"),
                "hello!",
                test_type,
                1,
                &response1);

    DLOG(INFO) << "Sending X-Associated-Content request";

    // This value should be set to something later than the one in
    // 'response1.request_time'.
    base::Time request1_time = writes[0].time_stamp;
    // We don't have a |writes| entry for the second request,
    // so put in Now() as the request time.  It's not as accurate,
    // but it will work.
    base::Time request2_time = base::Time::Now();

    // Issue a second request for the X-Associated-Content.
    MakeRequest(session,
                data,
                GURL("http://www.google.com/foo.dat"),
                syn_body_data,
                test_type,
                2,
                &response2);

    // Complete the next read now and teardown.
    data->CompleteRead();

    // Verify that we consumed all test data.
    EXPECT_TRUE(data->at_read_eof());
    EXPECT_TRUE(data->at_write_eof());

    // Check the timings

    // Verify that all the time stamps were set.
    EXPECT_GE(response1.request_time.ToInternalValue(),
        zero_time.ToInternalValue());
    EXPECT_GE(response2.request_time.ToInternalValue(),
        zero_time.ToInternalValue());
    EXPECT_GE(response1.response_time.ToInternalValue(),
        zero_time.ToInternalValue());
    EXPECT_GE(response2.response_time.ToInternalValue(),
        zero_time.ToInternalValue());

    // Verify that the values make sense.
    // First request.
    EXPECT_LE(response1.request_time.ToInternalValue(),
        request1_time.ToInternalValue());
    EXPECT_LE(response1.response_time.ToInternalValue(),
        reads[1].time_stamp.ToInternalValue());

    // Push request.
    EXPECT_GE(response2.request_time.ToInternalValue(),
        request2_time.ToInternalValue());
    // The response time should be between the server push SYN and DATA.
    EXPECT_GE(response2.response_time.ToInternalValue(),
      reads[first_push_data_frame - 1].time_stamp.ToInternalValue());
    EXPECT_LE(response2.response_time.ToInternalValue(),
      reads[first_push_data_frame].time_stamp.ToInternalValue());
  }
}

// Test that we shutdown correctly on write errors.
TEST_F(SpdyNetworkTransactionTest, WriteError) {
  MockWrite writes[] = {
    // We'll write 10 bytes successfully
    MockWrite(true, reinterpret_cast<const char*>(kGetSyn), 10),
    // Followed by ERROR!
    MockWrite(true, ERR_FAILED),
    MockWrite(true, 0, 0)  // EOF
  };

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(kGetSynReply),
             arraysize(kGetSynReply)),
    MockRead(true, reinterpret_cast<const char*>(kGetBodyFrame),
             arraysize(kGetBodyFrame)),
    MockRead(true, 0, 0)  // EOF
  };

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;
  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(2, reads, arraysize(reads),
                            writes, arraysize(writes)));
  TransactionHelperResult out = TransactionHelper(request, data.get(),
                                                  BoundNetLog());
  EXPECT_EQ(ERR_FAILED, out.rv);
  data->Reset();
}

// Test that partial writes work.
TEST_F(SpdyNetworkTransactionTest, PartialWrite) {
  // Chop the SYN_STREAM frame into 5 chunks.
  const int kChunks = 5;
  scoped_array<MockWrite> writes(ChopFrame(
      reinterpret_cast<const char*>(kGetSyn), arraysize(kGetSyn), kChunks));

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(kGetSynReply),
             arraysize(kGetSynReply)),
    MockRead(true, reinterpret_cast<const char*>(kGetBodyFrame),
             arraysize(kGetBodyFrame)),
    MockRead(true, 0, 0)  // EOF
  };

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;
  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(kChunks, reads, arraysize(reads),
                            writes.get(), kChunks));
  TransactionHelperResult out = TransactionHelper(request, data.get(),
                                                  BoundNetLog());
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

TEST_F(SpdyNetworkTransactionTest, ConnectFailure) {
  MockConnect connects[]  = {
    MockConnect(true, ERR_NAME_NOT_RESOLVED),
    MockConnect(false, ERR_NAME_NOT_RESOLVED),
    MockConnect(true, ERR_INTERNET_DISCONNECTED),
    MockConnect(false, ERR_INTERNET_DISCONNECTED)
  };

  for (size_t index = 0; index < arraysize(connects); ++index) {
    MockWrite writes[] = {
      MockWrite(true, reinterpret_cast<const char*>(kGetSyn),
                arraysize(kGetSyn)),
      MockWrite(true, 0, 0)  // EOF
    };

    MockRead reads[] = {
      MockRead(true, reinterpret_cast<const char*>(kGetSynReply),
               arraysize(kGetSynReply)),
      MockRead(true, reinterpret_cast<const char*>(kGetBodyFrame),
               arraysize(kGetBodyFrame)),
      MockRead(true, 0, 0)  // EOF
    };

    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/");
    request.load_flags = 0;
    scoped_refptr<DelayedSocketData> data(
        new DelayedSocketData(connects[index], 1, reads, arraysize(reads),
                              writes, arraysize(writes)));
    TransactionHelperResult out = TransactionHelper(request, data.get(),
                                                    BoundNetLog());
    EXPECT_EQ(connects[index].result, out.rv);
  }
}

// In this test, we enable compression, but get a uncompressed SynReply from
// the server.  Verify that teardown is all clean.
TEST_F(SpdyNetworkTransactionTest, DecompressFailureOnSynReply) {
  MockWrite writes[] = {
    MockWrite(true, reinterpret_cast<const char*>(kGetSynCompressed),
              arraysize(kGetSynCompressed)),
    MockWrite(true, 0, 0)  // EOF
  };

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(kGetSynReply),
             arraysize(kGetSynReply)),
    MockRead(true, reinterpret_cast<const char*>(kGetBodyFrame),
             arraysize(kGetBodyFrame)),
    MockRead(true, 0, 0)  // EOF
  };

  // For this test, we turn on the normal compression.
  EnableCompression(true);

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;
  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  TransactionHelperResult out = TransactionHelper(request, data.get(),
                                                  BoundNetLog());
  EXPECT_EQ(ERR_SYN_REPLY_NOT_RECEIVED, out.rv);
  data->Reset();

  EnableCompression(false);
}

// Test that the NetLog contains good data for a simple GET request.
TEST_F(SpdyNetworkTransactionTest, NetLog) {
  MockWrite writes[] = {
    MockWrite(true, reinterpret_cast<const char*>(kGetSyn),
              arraysize(kGetSyn)),
  };

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(kGetSynReply),
             arraysize(kGetSynReply)),
    MockRead(true, reinterpret_cast<const char*>(kGetBodyFrame),
             arraysize(kGetBodyFrame)),
    MockRead(true, 0, 0)  // EOF
  };

  net::CapturingBoundNetLog log(net::CapturingNetLog::kUnbounded);

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;
  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  TransactionHelperResult out = TransactionHelper(request, data.get(),
                                                  log.bound());
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  // Check that the NetLog was filled reasonably.
  // This test is intentionally non-specific about the exact ordering of
  // the log; instead we just check to make sure that certain events exist.
  EXPECT_LT(0u, log.entries().size());
  int pos = 0;
  // We know the first event at position 0.
  EXPECT_TRUE(net::LogContainsBeginEvent(
      log.entries(), 0, net::NetLog::TYPE_SPDY_TRANSACTION_INIT_CONNECTION));
  // For the rest of the events, allow additional events in the middle,
  // but expect these to be logged in order.
  pos = net::ExpectLogContainsSomewhere(log.entries(), 0,
      net::NetLog::TYPE_SPDY_TRANSACTION_INIT_CONNECTION,
      net::NetLog::PHASE_END);
  pos = net::ExpectLogContainsSomewhere(log.entries(), pos + 1,
      net::NetLog::TYPE_SPDY_TRANSACTION_SEND_REQUEST,
      net::NetLog::PHASE_BEGIN);
  pos = net::ExpectLogContainsSomewhere(log.entries(), pos + 1,
      net::NetLog::TYPE_SPDY_TRANSACTION_SEND_REQUEST,
      net::NetLog::PHASE_END);
  pos = net::ExpectLogContainsSomewhere(log.entries(), pos + 1,
      net::NetLog::TYPE_SPDY_TRANSACTION_READ_HEADERS,
      net::NetLog::PHASE_BEGIN);
  pos = net::ExpectLogContainsSomewhere(log.entries(), pos + 1,
      net::NetLog::TYPE_SPDY_TRANSACTION_READ_HEADERS,
      net::NetLog::PHASE_END);
  pos = net::ExpectLogContainsSomewhere(log.entries(), pos + 1,
      net::NetLog::TYPE_SPDY_TRANSACTION_READ_BODY,
      net::NetLog::PHASE_BEGIN);
  pos = net::ExpectLogContainsSomewhere(log.entries(), pos + 1,
      net::NetLog::TYPE_SPDY_TRANSACTION_READ_BODY,
      net::NetLog::PHASE_END);
}

// Since we buffer the IO from the stream to the renderer, this test verifies
// that when we read out the maximum amount of data (e.g. we received 50 bytes
// on the network, but issued a Read for only 5 of those bytes) that the data
// flow still works correctly.
TEST_F(SpdyNetworkTransactionTest, BufferFull) {
  MockWrite writes[] = {
    MockWrite(true, reinterpret_cast<const char*>(kGetSyn),
              arraysize(kGetSyn)),
  };

  static const unsigned char kCombinedDataFrames[] = {
    0x00, 0x00, 0x00, 0x01,                                      // header
    0x00, 0x00, 0x00, 0x06,                                      // length
    'g', 'o', 'o', 'd', 'b', 'y',
    0x00, 0x00, 0x00, 0x01,                                      // header
    0x00, 0x00, 0x00, 0x06,                                      // length
    'e', ' ', 'w', 'o', 'r', 'l',
  };

  static const unsigned char kLastFrame[] = {
    0x00, 0x00, 0x00, 0x01,                                      // header
    0x01, 0x00, 0x00, 0x01,                                      // FIN, length
    'd',
  };

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(kGetSynReply),
             arraysize(kGetSynReply)),
    MockRead(true, ERR_IO_PENDING),  // Force a pause
    MockRead(true, reinterpret_cast<const char*>(kCombinedDataFrames),
             arraysize(kCombinedDataFrames)),
    MockRead(true, ERR_IO_PENDING),  // Force a pause
    MockRead(true, reinterpret_cast<const char*>(kLastFrame),
             arraysize(kLastFrame)),
    MockRead(true, 0, 0)  // EOF
  };

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;
  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));

  // For this test, we can't use the TransactionHelper, because we are
  // going to tightly control how the IOs fly.

  TransactionHelperResult out;

  // We disable SSL for this test.
  SpdySession::SetSSLMode(false);

  SessionDependencies session_deps;
  scoped_ptr<SpdyNetworkTransaction> trans(
      new SpdyNetworkTransaction(CreateSession(&session_deps)));

  session_deps.socket_factory.AddSocketDataProvider(data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  out.rv = callback.WaitForResult();
  EXPECT_EQ(out.rv, OK);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers != NULL);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  out.status_line = response->headers->GetStatusLine();
  out.response_info = *response;  // Make a copy so we can verify.

  // Read Data
  TestCompletionCallback read_callback;

  std::string content;
  do {
    // Read small chunks at a time.
    const int kSmallReadSize = 3;
    scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(kSmallReadSize);
    rv = trans->Read(buf, kSmallReadSize, &read_callback);
    if (rv == net::ERR_IO_PENDING) {
      data->CompleteRead();
      rv = read_callback.WaitForResult();
    }
    if (rv > 0) {
      content.append(buf->data(), rv);
    } else if (rv < 0) {
      NOTREACHED();
    }
  } while (rv > 0);

  out.response_data.swap(content);

  // Flush the MessageLoop while the SessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  MessageLoop::current()->RunAllPending();


  // Verify that we consumed all test data.
  EXPECT_TRUE(data->at_read_eof());
  EXPECT_TRUE(data->at_write_eof());

  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("goodbye world", out.response_data);
}

// Verify that basic buffering works; when multiple data frames arrive
// at the same time, ensure that we don't notify a read completion for
// each data frame individually.
TEST_F(SpdyNetworkTransactionTest, Buffering) {
  MockWrite writes[] = {
    MockWrite(true, reinterpret_cast<const char*>(kGetSyn),
              arraysize(kGetSyn)),
  };

  // 4 data frames in a single read.
  static const unsigned char kCombinedDataFrames[] = {
    0x00, 0x00, 0x00, 0x01,                                      // header
    0x00, 0x00, 0x00, 0x07,                                      // length
    'm', 'e', 's', 's', 'a', 'g', 'e',
    0x00, 0x00, 0x00, 0x01,                                      // header
    0x00, 0x00, 0x00, 0x07,                                      // length
    'm', 'e', 's', 's', 'a', 'g', 'e',
    0x00, 0x00, 0x00, 0x01,                                      // header
    0x00, 0x00, 0x00, 0x07,                                      // length
    'm', 'e', 's', 's', 'a', 'g', 'e',
    0x00, 0x00, 0x00, 0x01,                                      // header
    0x01, 0x00, 0x00, 0x07,                                      // FIN, length
    'm', 'e', 's', 's', 'a', 'g', 'e',
  };

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(kGetSynReply),
             arraysize(kGetSynReply)),
    MockRead(true, ERR_IO_PENDING),  // Force a pause
    MockRead(true, reinterpret_cast<const char*>(kCombinedDataFrames),
             arraysize(kCombinedDataFrames)),
    MockRead(true, 0, 0)  // EOF
  };

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;
  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));

  // For this test, we can't use the TransactionHelper, because we are
  // going to tightly control how the IOs fly.

  TransactionHelperResult out;

  // We disable SSL for this test.
  SpdySession::SetSSLMode(false);

  SessionDependencies session_deps;
  scoped_ptr<SpdyNetworkTransaction> trans(
      new SpdyNetworkTransaction(CreateSession(&session_deps)));

  session_deps.socket_factory.AddSocketDataProvider(data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  out.rv = callback.WaitForResult();
  EXPECT_EQ(out.rv, OK);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers != NULL);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  out.status_line = response->headers->GetStatusLine();
  out.response_info = *response;  // Make a copy so we can verify.

  // Read Data
  TestCompletionCallback read_callback;

  std::string content;
  int reads_completed = 0;
  do {
    // Read small chunks at a time.
    const int kSmallReadSize = 14;
    scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(kSmallReadSize);
    rv = trans->Read(buf, kSmallReadSize, &read_callback);
    if (rv == net::ERR_IO_PENDING) {
      data->CompleteRead();
      rv = read_callback.WaitForResult();
    }
    if (rv > 0) {
      EXPECT_EQ(kSmallReadSize, rv);
      content.append(buf->data(), rv);
    } else if (rv < 0) {
      FAIL() << "Unexpected read error: " << rv;
    }
    reads_completed++;
  } while (rv > 0);

  EXPECT_EQ(3, reads_completed);  // Reads are: 14 bytes, 14 bytes, 0 bytes.

  out.response_data.swap(content);

  // Flush the MessageLoop while the SessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  MessageLoop::current()->RunAllPending();

  // Verify that we consumed all test data.
  EXPECT_TRUE(data->at_read_eof());
  EXPECT_TRUE(data->at_write_eof());

  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("messagemessagemessagemessage", out.response_data);
}

// Verify the case where we buffer data but read it after it has been buffered.
TEST_F(SpdyNetworkTransactionTest, BufferedAll) {
  MockWrite writes[] = {
    MockWrite(true, reinterpret_cast<const char*>(kGetSyn),
              arraysize(kGetSyn)),
  };

  // The Syn Reply and all data frames in a single read.
  static const unsigned char kCombinedFrames[] = {
    0x80, 0x01, 0x00, 0x02,                                      // header
    0x00, 0x00, 0x00, 0x45,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x04,                                      // 4 headers
    0x00, 0x05, 'h', 'e', 'l', 'l', 'o',
    0x00, 0x03, 'b', 'y', 'e',
    0x00, 0x06, 's', 't', 'a', 't', 'u', 's',
    0x00, 0x03, '2', '0', '0',
    0x00, 0x03, 'u', 'r', 'l',
    0x00, 0x0a, '/', 'i', 'n', 'd', 'e', 'x', '.', 'p', 'h', 'p',
    0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',
    0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',
    0x00, 0x00, 0x00, 0x01,                                      // header
    0x00, 0x00, 0x00, 0x07,                                      // length
    'm', 'e', 's', 's', 'a', 'g', 'e',
    0x00, 0x00, 0x00, 0x01,                                      // header
    0x00, 0x00, 0x00, 0x07,                                      // length
    'm', 'e', 's', 's', 'a', 'g', 'e',
    0x00, 0x00, 0x00, 0x01,                                      // header
    0x00, 0x00, 0x00, 0x07,                                      // length
    'm', 'e', 's', 's', 'a', 'g', 'e',
    0x00, 0x00, 0x00, 0x01,                                      // header
    0x01, 0x00, 0x00, 0x07,                                      // FIN, length
    'm', 'e', 's', 's', 'a', 'g', 'e',
  };

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(kCombinedFrames),
             arraysize(kCombinedFrames)),
    MockRead(true, 0, 0)  // EOF
  };

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;
  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));

  // For this test, we can't use the TransactionHelper, because we are
  // going to tightly control how the IOs fly.

  TransactionHelperResult out;

  // We disable SSL for this test.
  SpdySession::SetSSLMode(false);

  SessionDependencies session_deps;
  scoped_ptr<SpdyNetworkTransaction> trans(
      new SpdyNetworkTransaction(CreateSession(&session_deps)));

  session_deps.socket_factory.AddSocketDataProvider(data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  out.rv = callback.WaitForResult();
  EXPECT_EQ(out.rv, OK);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers != NULL);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  out.status_line = response->headers->GetStatusLine();
  out.response_info = *response;  // Make a copy so we can verify.

  // Read Data
  TestCompletionCallback read_callback;

  std::string content;
  int reads_completed = 0;
  do {
    // Read small chunks at a time.
    const int kSmallReadSize = 14;
    scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(kSmallReadSize);
    rv = trans->Read(buf, kSmallReadSize, &read_callback);
    if (rv > 0) {
      EXPECT_EQ(kSmallReadSize, rv);
      content.append(buf->data(), rv);
    } else if (rv < 0) {
      FAIL() << "Unexpected read error: " << rv;
    }
    reads_completed++;
  } while (rv > 0);

  EXPECT_EQ(3, reads_completed);

  out.response_data.swap(content);

  // Flush the MessageLoop while the SessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  MessageLoop::current()->RunAllPending();

  // Verify that we consumed all test data.
  EXPECT_TRUE(data->at_read_eof());
  EXPECT_TRUE(data->at_write_eof());

  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("messagemessagemessagemessage", out.response_data);
}

// Verify the case where we buffer data and close the connection.
TEST_F(SpdyNetworkTransactionTest, BufferedClosed) {
  MockWrite writes[] = {
    MockWrite(true, reinterpret_cast<const char*>(kGetSyn),
              arraysize(kGetSyn)),
  };

  // All data frames in a single read.
  static const unsigned char kCombinedFrames[] = {
    0x00, 0x00, 0x00, 0x01,                                      // header
    0x00, 0x00, 0x00, 0x07,                                      // length
    'm', 'e', 's', 's', 'a', 'g', 'e',
    0x00, 0x00, 0x00, 0x01,                                      // header
    0x00, 0x00, 0x00, 0x07,                                      // length
    'm', 'e', 's', 's', 'a', 'g', 'e',
    0x00, 0x00, 0x00, 0x01,                                      // header
    0x00, 0x00, 0x00, 0x07,                                      // length
    'm', 'e', 's', 's', 'a', 'g', 'e',
    0x00, 0x00, 0x00, 0x01,                                      // header
    0x00, 0x00, 0x00, 0x07,                                      // length
    'm', 'e', 's', 's', 'a', 'g', 'e',
    // NOTE: We didn't FIN the stream.
  };

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(kGetSynReply),
             arraysize(kGetSynReply)),
    MockRead(true, ERR_IO_PENDING),  // Force a wait
    MockRead(true, reinterpret_cast<const char*>(kCombinedFrames),
             arraysize(kCombinedFrames)),
    MockRead(true, 0, 0)  // EOF
  };

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;
  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));

  // For this test, we can't use the TransactionHelper, because we are
  // going to tightly control how the IOs fly.

  TransactionHelperResult out;

  // We disable SSL for this test.
  SpdySession::SetSSLMode(false);

  SessionDependencies session_deps;
  scoped_ptr<SpdyNetworkTransaction> trans(
      new SpdyNetworkTransaction(CreateSession(&session_deps)));

  session_deps.socket_factory.AddSocketDataProvider(data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  out.rv = callback.WaitForResult();
  EXPECT_EQ(out.rv, OK);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers != NULL);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  out.status_line = response->headers->GetStatusLine();
  out.response_info = *response;  // Make a copy so we can verify.

  // Read Data
  TestCompletionCallback read_callback;

  std::string content;
  int reads_completed = 0;
  do {
    // Read small chunks at a time.
    const int kSmallReadSize = 14;
    scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(kSmallReadSize);
    rv = trans->Read(buf, kSmallReadSize, &read_callback);
    if (rv == net::ERR_IO_PENDING) {
      data->CompleteRead();
      rv = read_callback.WaitForResult();
    }
    if (rv > 0) {
      content.append(buf->data(), rv);
    } else if (rv < 0) {
      // This test intentionally closes the connection, and will get an error.
      EXPECT_EQ(ERR_CONNECTION_CLOSED, rv);
      break;
    }
    reads_completed++;
  } while (rv > 0);

  EXPECT_EQ(0, reads_completed);

  out.response_data.swap(content);

  // Flush the MessageLoop while the SessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  MessageLoop::current()->RunAllPending();

  // Verify that we consumed all test data.
  EXPECT_TRUE(data->at_read_eof());
  EXPECT_TRUE(data->at_write_eof());
}

// Verify the case where we buffer data and cancel the transaction.
TEST_F(SpdyNetworkTransactionTest, BufferedCancelled) {
  MockWrite writes[] = {
    MockWrite(true, reinterpret_cast<const char*>(kGetSyn),
              arraysize(kGetSyn)),
  };

  static const unsigned char kDataFrame[] = {
    0x00, 0x00, 0x00, 0x01,                                      // header
    0x00, 0x00, 0x00, 0x07,                                      // length
    'm', 'e', 's', 's', 'a', 'g', 'e',
    // NOTE: We didn't FIN the stream.
  };

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(kGetSynReply),
             arraysize(kGetSynReply)),
    MockRead(true, ERR_IO_PENDING),  // Force a wait
    MockRead(true, reinterpret_cast<const char*>(kDataFrame),
             arraysize(kDataFrame)),
    MockRead(true, 0, 0)  // EOF
  };

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;
  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));

  // For this test, we can't use the TransactionHelper, because we are
  // going to tightly control how the IOs fly.
  TransactionHelperResult out;

  // We disable SSL for this test.
  SpdySession::SetSSLMode(false);

  SessionDependencies session_deps;
  scoped_ptr<SpdyNetworkTransaction> trans(
      new SpdyNetworkTransaction(CreateSession(&session_deps)));

  session_deps.socket_factory.AddSocketDataProvider(data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  out.rv = callback.WaitForResult();
  EXPECT_EQ(out.rv, OK);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response->headers != NULL);
  EXPECT_TRUE(response->was_fetched_via_spdy);
  out.status_line = response->headers->GetStatusLine();
  out.response_info = *response;  // Make a copy so we can verify.

  // Read Data
  TestCompletionCallback read_callback;

  do {
    const int kReadSize = 256;
    scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(kReadSize);
    rv = trans->Read(buf, kReadSize, &read_callback);
    if (rv == net::ERR_IO_PENDING) {
      // Complete the read now, which causes buffering to start.
      data->CompleteRead();
      // Destroy the transaction, causing the stream to get cancelled
      // and orphaning the buffered IO task.
      trans.reset();
      break;
    }
    // We shouldn't get here in this test.
    FAIL() << "Unexpected read: " << rv;
  } while (rv > 0);

  // Flush the MessageLoop; this will cause the buffered IO task
  // to run for the final time.
  MessageLoop::current()->RunAllPending();
}

// Test that if the server requests persistence of settings, that we save
// the settings in the SpdySettingsStorage.
TEST_F(SpdyNetworkTransactionTest, SettingsSaved) {
  static const SpdyHeaderInfo kSynReplyInfo = {
    spdy::SYN_REPLY,                              // Syn Reply
    1,                                            // Stream ID
    0,                                            // Associated Stream ID
    3,                                            // Priority
    spdy::CONTROL_FLAG_NONE,                      // Control Flags
    false,                                        // Compressed
    200,                                          // Status
    NULL,                                         // Data
    0,                                            // Data Length
    spdy::DATA_FLAG_NONE                          // Data Flags
  };
  static const char* const kExtraHeaders[] = {
    "status",   "200",
    "version",  "HTTP/1.1"
  };

  SessionDependencies session_deps;
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));

  // Verify that no settings exist initially.
  HostPortPair host_port_pair("www.google.com", 80);
  EXPECT_TRUE(session->spdy_settings().Get(host_port_pair).empty());

  // Construct the request.
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0));

  MockWrite writes[] = {
    MockWrite(true, req->data(), req->length() + spdy::SpdyFrame::size()),
  };

  // Construct the reply.
  scoped_ptr<spdy::SpdyFrame> reply(
    ConstructSpdyPacket(&kSynReplyInfo,
                        kExtraHeaders,
                        arraysize(kExtraHeaders) / 2,
                        NULL,
                        0));

  unsigned int kSampleId1 = 0x1;
  unsigned int kSampleValue1 = 0x0a0a0a0a;
  unsigned int kSampleId2 = 0x2;
  unsigned int kSampleValue2 = 0x0b0b0b0b;
  unsigned int kSampleId3 = 0xababab;
  unsigned int kSampleValue3 = 0x0c0c0c0c;
  scoped_ptr<spdy::SpdyFrame> settings_frame;
  {
    // Construct the SETTINGS frame.
    spdy::SpdySettings settings;
    spdy::SettingsFlagsAndId setting(0);
    // First add a persisted setting
    setting.set_flags(spdy::SETTINGS_FLAG_PLEASE_PERSIST);
    setting.set_id(kSampleId1);
    settings.push_back(std::make_pair(setting, kSampleValue1));
    // Next add a non-persisted setting
    setting.set_flags(0);
    setting.set_id(kSampleId2);
    settings.push_back(std::make_pair(setting, kSampleValue2));
    // Next add another persisted setting
    setting.set_flags(spdy::SETTINGS_FLAG_PLEASE_PERSIST);
    setting.set_id(kSampleId3);
    settings.push_back(std::make_pair(setting, kSampleValue3));
    settings_frame.reset(ConstructSpdySettings(settings));
  }

  MockRead reads[] = {
    MockRead(true, reply->data(), reply->length() + spdy::SpdyFrame::size()),
    MockRead(true, reinterpret_cast<const char*>(kGetBodyFrame),
             arraysize(kGetBodyFrame)),
    MockRead(true, settings_frame->data(),
             settings_frame->length() + spdy::SpdyFrame::size()),
    MockRead(true, 0, 0)  // EOF
  };

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  TransactionHelperResult out = TransactionHelperWithSession(request,
                                                             data.get(),
                                                             BoundNetLog(),
                                                             &session_deps,
                                                             session.get());
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  {
    // Verify we had two persisted settings.
    spdy::SpdySettings saved_settings =
        session->spdy_settings().Get(host_port_pair);
    ASSERT_EQ(2u, saved_settings.size());

    // Verify the first persisted setting.
    spdy::SpdySetting setting = saved_settings.front();
    saved_settings.pop_front();
    EXPECT_EQ(spdy::SETTINGS_FLAG_PERSISTED, setting.first.flags());
    EXPECT_EQ(kSampleId1, setting.first.id());
    EXPECT_EQ(kSampleValue1, setting.second);

    // Verify the second persisted setting.
    setting = saved_settings.front();
    saved_settings.pop_front();
    EXPECT_EQ(spdy::SETTINGS_FLAG_PERSISTED, setting.first.flags());
    EXPECT_EQ(kSampleId3, setting.first.id());
    EXPECT_EQ(kSampleValue3, setting.second);
  }
}

// Test that when there are settings saved that they are sent back to the
// server upon session establishment.
TEST_F(SpdyNetworkTransactionTest, SettingsPlayback) {
  static const SpdyHeaderInfo kSynReplyInfo = {
    spdy::SYN_REPLY,                              // Syn Reply
    1,                                            // Stream ID
    0,                                            // Associated Stream ID
    3,                                            // Priority
    spdy::CONTROL_FLAG_NONE,                      // Control Flags
    false,                                        // Compressed
    200,                                          // Status
    NULL,                                         // Data
    0,                                            // Data Length
    spdy::DATA_FLAG_NONE                          // Data Flags
  };
  static const char* kExtraHeaders[] = {
    "status",   "200",
    "version",  "HTTP/1.1"
  };

  SessionDependencies session_deps;
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));

  // Verify that no settings exist initially.
  HostPortPair host_port_pair("www.google.com", 80);
  EXPECT_TRUE(session->spdy_settings().Get(host_port_pair).empty());

  unsigned int kSampleId1 = 0x1;
  unsigned int kSampleValue1 = 0x0a0a0a0a;
  unsigned int kSampleId2 = 0xababab;
  unsigned int kSampleValue2 = 0x0c0c0c0c;
  // Manually insert settings into the SpdySettingsStorage here.
  {
    spdy::SpdySettings settings;
    spdy::SettingsFlagsAndId setting(0);
    // First add a persisted setting
    setting.set_flags(spdy::SETTINGS_FLAG_PLEASE_PERSIST);
    setting.set_id(kSampleId1);
    settings.push_back(std::make_pair(setting, kSampleValue1));
    // Next add another persisted setting
    setting.set_flags(spdy::SETTINGS_FLAG_PLEASE_PERSIST);
    setting.set_id(kSampleId2);
    settings.push_back(std::make_pair(setting, kSampleValue2));

    session->mutable_spdy_settings()->Set(host_port_pair, settings);
  }

  EXPECT_EQ(2u, session->spdy_settings().Get(host_port_pair).size());

  // Construct the SETTINGS frame.
  const spdy::SpdySettings& settings =
      session->spdy_settings().Get(host_port_pair);
  scoped_ptr<spdy::SpdyFrame> settings_frame(ConstructSpdySettings(settings));

  // Construct the request.
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyGet(NULL, 0));

  MockWrite writes[] = {
    MockWrite(true, settings_frame->data(),
              settings_frame->length() + spdy::SpdyFrame::size()),
    MockWrite(true, req->data(), req->length() + spdy::SpdyFrame::size()),
  };

  // Construct the reply.
  scoped_ptr<spdy::SpdyFrame> reply(
    ConstructSpdyPacket(&kSynReplyInfo,
                        kExtraHeaders,
                        arraysize(kExtraHeaders) / 2,
                        NULL,
                        0));

  MockRead reads[] = {
    MockRead(true, reply->data(), reply->length() + spdy::SpdyFrame::size()),
    MockRead(true, reinterpret_cast<const char*>(kGetBodyFrame),
             arraysize(kGetBodyFrame)),
    MockRead(true, 0, 0)  // EOF
  };

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(2, reads, arraysize(reads),
                            writes, arraysize(writes)));
  TransactionHelperResult out = TransactionHelperWithSession(request,
                                                             data.get(),
                                                             BoundNetLog(),
                                                             &session_deps,
                                                             session.get());
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);

  {
    // Verify we had two persisted settings.
    spdy::SpdySettings saved_settings =
        session->spdy_settings().Get(host_port_pair);
    ASSERT_EQ(2u, saved_settings.size());

    // Verify the first persisted setting.
    spdy::SpdySetting setting = saved_settings.front();
    saved_settings.pop_front();
    EXPECT_EQ(spdy::SETTINGS_FLAG_PERSISTED, setting.first.flags());
    EXPECT_EQ(kSampleId1, setting.first.id());
    EXPECT_EQ(kSampleValue1, setting.second);

    // Verify the second persisted setting.
    setting = saved_settings.front();
    saved_settings.pop_front();
    EXPECT_EQ(spdy::SETTINGS_FLAG_PERSISTED, setting.first.flags());
    EXPECT_EQ(kSampleId2, setting.first.id());
    EXPECT_EQ(kSampleValue2, setting.second);
  }
}

TEST_F(SpdyNetworkTransactionTest, GoAwayWithActiveStream) {
  MockWrite writes[] = {
    MockWrite(true, reinterpret_cast<const char*>(kGetSyn),
              arraysize(kGetSyn)),
  };

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(kGoAway),
             arraysize(kGoAway)),
    MockRead(true, 0, 0)  // EOF
  };

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;
  scoped_refptr<DelayedSocketData> data(
      new DelayedSocketData(1, reads, arraysize(reads),
                            writes, arraysize(writes)));
  TransactionHelperResult out = TransactionHelper(request, data.get(),
                                                  BoundNetLog());
  EXPECT_EQ(ERR_CONNECTION_CLOSED, out.rv);
}

}  // namespace net
