// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_http_stream.h"

#include <vector>

#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_response_headers.h"
#include "net/quic/quic_client_session.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_connection_helper.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/test_task_runner.h"
#include "net/socket/socket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace net {

class QuicConnectionPeer {
 public:
  static void SetScheduler(QuicConnection* connection,
                           QuicSendScheduler* scheduler) {
    connection->scheduler_.reset(scheduler);
  }

  static void SetCollector(QuicConnection* connection,
                           QuicReceiptMetricsCollector* collector) {
    connection->collector_.reset(collector);
  }
};

namespace test {

namespace {

const char kUploadData[] = "hello world!";

class TestQuicConnection : public QuicConnection {
 public:
  TestQuicConnection(QuicGuid guid,
                     IPEndPoint address,
                     QuicConnectionHelper* helper)
      : QuicConnection(guid, address, helper) {
  }

  void SetScheduler(QuicSendScheduler* scheduler) {
    QuicConnectionPeer::SetScheduler(this, scheduler);
  }

  void SetCollector(QuicReceiptMetricsCollector* collector) {
    QuicConnectionPeer::SetCollector(this, collector);
  }
};

class TestCollector : public QuicReceiptMetricsCollector {
 public:
  explicit TestCollector(QuicCongestionFeedbackFrame* feedback)
      : QuicReceiptMetricsCollector(&clock_, kFixRate),
        feedback_(feedback) {
  }

  bool GenerateCongestionFeedback(
      QuicCongestionFeedbackFrame* congestion_feedback) {
    if (feedback_ == NULL) {
      return false;
    }
    *congestion_feedback = *feedback_;
    return true;
  }

  MOCK_METHOD4(RecordIncomingPacket,
               void(size_t, QuicPacketSequenceNumber, QuicTime, bool));

 private:
  MockClock clock_;
  QuicCongestionFeedbackFrame* feedback_;

  DISALLOW_COPY_AND_ASSIGN(TestCollector);
};

}  // namespace

class QuicHttpStreamTest : public ::testing::Test {
 protected:
  const static bool kFin = true;
  const static bool kNoFin = false;
  // Holds a packet to be written to the wire, and the IO mode that should
  // be used by the mock socket when performing the write.
  struct PacketToWrite {
    PacketToWrite(IoMode mode, QuicEncryptedPacket* packet)
        : mode(mode),
          packet(packet) {
    }
    IoMode mode;
    QuicEncryptedPacket* packet;
  };

  QuicHttpStreamTest()
      : net_log_(BoundNetLog()),
        read_buffer_(new IOBufferWithSize(4096)),
        guid_(2),
        framer_(QuicDecrypter::Create(kNULL), QuicEncrypter::Create(kNULL)),
        creator_(guid_, &framer_) {
    IPAddressNumber ip;
    CHECK(ParseIPLiteralToNumber("192.0.2.33", &ip));
    peer_addr_ = IPEndPoint(ip, 443);
    self_addr_ = IPEndPoint(ip, 8435);
    // Do null initialization for simple tests.
    Initialize();
  }

  ~QuicHttpStreamTest() {
    for (size_t i = 0; i < writes_.size(); i++) {
      delete writes_[i].packet;
    }
  }

  // Adds a packet to the list of expected writes.
  void AddWrite(IoMode mode, QuicEncryptedPacket* packet) {
    writes_.push_back(PacketToWrite(mode, packet));
  }

  // Returns the packet to be written at position |pos|.
  QuicEncryptedPacket* GetWrite(size_t pos) {
    return writes_[pos].packet;
  }

  bool AtEof() {
    return socket_data_->at_read_eof() && socket_data_->at_write_eof();
  }

  void ProcessPacket(const QuicEncryptedPacket& packet) {
    connection_->ProcessUdpPacket(self_addr_, peer_addr_, packet);
  }

  // Configures the test fixture to use the list of expected writes.
  void Initialize() {
    mock_writes_.reset(new MockWrite[writes_.size()]);
    for (size_t i = 0; i < writes_.size(); i++) {
      mock_writes_[i] = MockWrite(writes_[i].mode,
                                  writes_[i].packet->data(),
                                  writes_[i].packet->length());
    };

    socket_data_.reset(new StaticSocketDataProvider(NULL, 0, mock_writes_.get(),
                                                    writes_.size()));

    MockUDPClientSocket* socket = new MockUDPClientSocket(socket_data_.get(),
                                                          net_log_.net_log());
    socket->Connect(peer_addr_);
    runner_ = new TestTaskRunner(&clock_);
    scheduler_ = new MockScheduler();
    collector_ = new TestCollector(NULL);
    EXPECT_CALL(*scheduler_, TimeUntilSend(_)).
        WillRepeatedly(testing::Return(QuicTime::Delta()));
    helper_ = new QuicConnectionHelper(runner_.get(), &clock_, socket);
    connection_ = new TestQuicConnection(guid_, peer_addr_, helper_);
    connection_->set_visitor(&visitor_);
    connection_->SetScheduler(scheduler_);
    connection_->SetCollector(collector_);
    session_.reset(new QuicClientSession(connection_, helper_, NULL));
    CryptoHandshakeMessage message;
    message.tag = kSHLO;
    session_->GetCryptoStream()->OnHandshakeMessage(message);
    EXPECT_TRUE(session_->IsCryptoHandshakeComplete());
    stream_.reset(new QuicHttpStream(session_->CreateOutgoingReliableStream()));
  }

  // Returns a newly created packet to send kData on stream 1.
  QuicEncryptedPacket* ConstructDataPacket(
      QuicPacketSequenceNumber sequence_number,
      bool fin,
      QuicStreamOffset offset,
      base::StringPiece data) {
    InitializeHeader(sequence_number);
    QuicStreamFrame frame(3, fin, offset, data);
    return ConstructPacket(header_, QuicFrame(&frame));
  }

  // Returns a newly created packet to send ack data.
  QuicEncryptedPacket* ConstructAckPacket(
      QuicPacketSequenceNumber sequence_number,
      QuicPacketSequenceNumber largest_received) {
    InitializeHeader(sequence_number);

    QuicAckFrame ack(largest_received, sequence_number);
    return ConstructPacket(header_, QuicFrame(&ack));
  }

  BoundNetLog net_log_;
  MockScheduler* scheduler_;
  TestCollector* collector_;
  scoped_refptr<TestTaskRunner> runner_;
  scoped_array<MockWrite> mock_writes_;
  MockClock clock_;
  TestQuicConnection* connection_;
  QuicConnectionHelper* helper_;
  testing::StrictMock<MockConnectionVisitor> visitor_;
  scoped_ptr<QuicHttpStream> stream_;
  scoped_ptr<QuicClientSession> session_;
  TestCompletionCallback callback_;
  HttpRequestInfo request_;
  HttpRequestHeaders headers_;
  HttpResponseInfo response_;
  scoped_refptr<IOBufferWithSize> read_buffer_;

 private:
  void InitializeHeader(QuicPacketSequenceNumber sequence_number) {
    header_.guid = guid_;
    header_.packet_sequence_number = sequence_number;
    header_.flags = PACKET_FLAGS_NONE;
    header_.fec_group = 0;
  }

  QuicEncryptedPacket* ConstructPacket(const QuicPacketHeader& header,
                                       const QuicFrame& frame) {
    QuicFrames frames;
    frames.push_back(frame);
    QuicPacket* packet;
    framer_.ConstructFrameDataPacket(header_, frames, &packet);
    QuicEncryptedPacket* encrypted = framer_.EncryptPacket(*packet);
    delete packet;
    return encrypted;
  }

  const QuicGuid guid_;
  QuicFramer framer_;
  IPEndPoint self_addr_;
  IPEndPoint peer_addr_;
  QuicPacketCreator creator_;
  QuicPacketHeader header_;
  scoped_ptr<StaticSocketDataProvider> socket_data_;
  std::vector<PacketToWrite> writes_;
};

TEST_F(QuicHttpStreamTest, RenewStreamForAuth) {
  EXPECT_EQ(NULL, stream_->RenewStreamForAuth());
}

TEST_F(QuicHttpStreamTest, CanFindEndOfResponse) {
  EXPECT_TRUE(stream_->CanFindEndOfResponse());
}

TEST_F(QuicHttpStreamTest, IsMoreDataBuffered) {
  EXPECT_FALSE(stream_->IsMoreDataBuffered());
}

TEST_F(QuicHttpStreamTest, IsConnectionReusable) {
  EXPECT_FALSE(stream_->IsConnectionReusable());
}

TEST_F(QuicHttpStreamTest, GetRequest) {
  AddWrite(SYNCHRONOUS, ConstructDataPacket(1, kFin, 0,
                                            "GET / HTTP/1.1\r\n\r\n"));
  AddWrite(SYNCHRONOUS, ConstructAckPacket(2, 2));
  Initialize();

  request_.method = "GET";
  request_.url = GURL("http://www.google.com/");

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, net_log_,
                                          callback_.callback()));
  EXPECT_EQ(OK, stream_->SendRequest(headers_, &response_,
                                     callback_.callback()));
  EXPECT_EQ(&response_, stream_->GetResponseInfo());

  // Ack the request.
  scoped_ptr<QuicEncryptedPacket> ack(ConstructAckPacket(1, 1));
  ProcessPacket(*ack);

  EXPECT_EQ(ERR_IO_PENDING,
            stream_->ReadResponseHeaders(callback_.callback()));

  // Send the response without a body.
  const char kResponseHeaders[] = "HTTP/1.1 404 OK\r\n"
      "Content-Type: text/plain\r\n\r\n";
  scoped_ptr<QuicEncryptedPacket> resp(
      ConstructDataPacket(2, kFin, 0, kResponseHeaders));
  ProcessPacket(*resp);

  // Now that the headers have been processed, the callback will return.
  EXPECT_EQ(OK, callback_.WaitForResult());
  ASSERT_TRUE(response_.headers != NULL);
  EXPECT_EQ(404, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));

  // There is no body, so this should return immediately.
  EXPECT_EQ(0, stream_->ReadResponseBody(read_buffer_.get(),
                                         read_buffer_->size(),
                                         callback_.callback()));
  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());
}

TEST_F(QuicHttpStreamTest, GetRequestFullResponseInSinglePacket) {
  AddWrite(SYNCHRONOUS, ConstructDataPacket(1, kFin, 0,
                                            "GET / HTTP/1.1\r\n\r\n"));
  AddWrite(SYNCHRONOUS, ConstructAckPacket(2, 2));
  Initialize();

  request_.method = "GET";
  request_.url = GURL("http://www.google.com/");

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, net_log_,
                                          callback_.callback()));
  EXPECT_EQ(OK, stream_->SendRequest(headers_, &response_,
                                     callback_.callback()));
  EXPECT_EQ(&response_, stream_->GetResponseInfo());

  // Ack the request.
  scoped_ptr<QuicEncryptedPacket> ack(ConstructAckPacket(1, 1));
  ProcessPacket(*ack);

  EXPECT_EQ(ERR_IO_PENDING,
            stream_->ReadResponseHeaders(callback_.callback()));

  // Send the response with a body.
  const char kResponseHeaders[] = "HTTP/1.1 404 OK\r\n"
      "Content-Type: text/plain\r\n\r\nhello world!";
  scoped_ptr<QuicEncryptedPacket> resp(
      ConstructDataPacket(2, kFin, 0, kResponseHeaders));
  ProcessPacket(*resp);

  // Now that the headers have been processed, the callback will return.
  EXPECT_EQ(OK, callback_.WaitForResult());
  ASSERT_TRUE(response_.headers != NULL);
  EXPECT_EQ(404, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));

  // There is no body, so this should return immediately.
  // Since the body has already arrived, this should return immediately.
  EXPECT_EQ(12, stream_->ReadResponseBody(read_buffer_.get(),
                                          read_buffer_->size(),
                                          callback_.callback()));
  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());
}

TEST_F(QuicHttpStreamTest, SendPostRequest) {
  const char kRequestData[] = "POST / HTTP/1.1\r\n\r\n";
  AddWrite(SYNCHRONOUS, ConstructDataPacket(1, kNoFin, 0, kRequestData));
  AddWrite(SYNCHRONOUS, ConstructDataPacket(2, kFin, strlen(kRequestData),
                                            kUploadData));
  AddWrite(SYNCHRONOUS, ConstructAckPacket(3, 2));
  AddWrite(SYNCHRONOUS, ConstructAckPacket(4, 3));

  Initialize();

  ScopedVector<UploadElementReader> element_readers;
  element_readers.push_back(
      new UploadBytesElementReader(kUploadData, strlen(kUploadData)));
  UploadDataStream upload_data_stream(&element_readers, 0);
  request_.method = "POST";
  request_.url = GURL("http://www.google.com/");
  request_.upload_data_stream = &upload_data_stream;
  ASSERT_EQ(OK, request_.upload_data_stream->InitSync());

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, net_log_,
                                          callback_.callback()));
  EXPECT_EQ(OK, stream_->SendRequest(headers_, &response_,
                                     callback_.callback()));
  EXPECT_EQ(&response_, stream_->GetResponseInfo());

  // Ack both packets in the request.
  scoped_ptr<QuicEncryptedPacket> ack(ConstructAckPacket(1, 2));
  ProcessPacket(*ack);

  // Send the response headers (but not the body).
  const char kResponseHeaders[] = "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n\r\n";
  scoped_ptr<QuicEncryptedPacket> resp(
      ConstructDataPacket(2, kNoFin, 0, kResponseHeaders));
  ProcessPacket(*resp);

  // Since the headers have already arrived, this should return immediately.
  EXPECT_EQ(OK, stream_->ReadResponseHeaders(callback_.callback()));
  ASSERT_TRUE(response_.headers != NULL);
  EXPECT_EQ(200, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));

  // Send the response body.
  const char kResponseBody[] = "Hello world!";
  scoped_ptr<QuicEncryptedPacket> resp_body(
      ConstructDataPacket(3, kFin, strlen(kResponseHeaders), kResponseBody));
  ProcessPacket(*resp_body);

  // Since the body has already arrived, this should return immediately.
  EXPECT_EQ(static_cast<int>(strlen(kResponseBody)),
            stream_->ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                      callback_.callback()));

  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());
}

}  // namespace test

}  // namespace net
