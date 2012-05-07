// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_transaction.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/rand_util.h"
#include "base/sys_byteorder.h"
#include "base/test/test_timeouts.h"
#include "net/base/big_endian.h"
#include "net/base/dns_util.h"
#include "net/base/net_log.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_session.h"
#include "net/dns/dns_test_util.h"
#include "net/socket/socket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

std::string DomainFromDot(const base::StringPiece& dotted) {
  std::string out;
  EXPECT_TRUE(DNSDomainFromDot(dotted, &out));
  return out;
}

class TestSocketFactory;

// A variant of MockUDPClientSocket which notifies the factory OnConnect.
class TestUDPClientSocket : public MockUDPClientSocket {
 public:
  TestUDPClientSocket(TestSocketFactory* factory,
                      SocketDataProvider* data,
                      net::NetLog* net_log)
      : MockUDPClientSocket(data, net_log), factory_(factory) {
  }
  virtual ~TestUDPClientSocket() {}
  virtual int Connect(const IPEndPoint& endpoint) OVERRIDE;
 private:
  TestSocketFactory* factory_;
};

// Creates TestUDPClientSockets and keeps endpoints reported via OnConnect.
class TestSocketFactory : public MockClientSocketFactory {
 public:
  TestSocketFactory() {}
  virtual ~TestSocketFactory() {}

  virtual DatagramClientSocket* CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      const RandIntCallback& rand_int_cb,
      net::NetLog* net_log,
      const net::NetLog::Source& source) OVERRIDE {
    SocketDataProvider* data_provider = mock_data().GetNext();
    TestUDPClientSocket* socket = new TestUDPClientSocket(this,
                                                          data_provider,
                                                          net_log);
    data_provider->set_socket(socket);
    return socket;
  }

  void OnConnect(const IPEndPoint& endpoint) {
    remote_endpoints.push_back(endpoint);
  }

  std::vector<IPEndPoint> remote_endpoints;
};

int TestUDPClientSocket::Connect(const IPEndPoint& endpoint) {
  factory_->OnConnect(endpoint);
  return MockUDPClientSocket::Connect(endpoint);
}

// Helper class that holds a DnsTransaction and handles OnTransactionComplete.
class TransactionHelper {
 public:
  // If |expected_answer_count| < 0 then it is the expected net error.
  TransactionHelper(const char* hostname,
                    uint16 qtype,
                    int expected_answer_count)
      : hostname_(hostname),
        qtype_(qtype),
        expected_answer_count_(expected_answer_count),
        cancel_in_callback_(false),
        quit_in_callback_(false),
        completed_(false) {
  }

  // Mark that the transaction shall be destroyed immediately upon callback.
  void set_cancel_in_callback() {
    cancel_in_callback_ = true;
  }

  // Mark to call MessageLoop::Quit() upon callback.
  void set_quit_in_callback() {
    quit_in_callback_ = true;
  }

  void StartTransaction(DnsTransactionFactory* factory) {
    EXPECT_EQ(NULL, transaction_.get());
    transaction_ = factory->CreateTransaction(
        hostname_,
        qtype_,
        base::Bind(&TransactionHelper::OnTransactionComplete,
                   base::Unretained(this)),
        BoundNetLog());
    EXPECT_EQ(hostname_, transaction_->GetHostname());
    EXPECT_EQ(qtype_, transaction_->GetType());
    int rv = transaction_->Start();
    if (rv != ERR_IO_PENDING) {
      EXPECT_NE(OK, rv);
      EXPECT_EQ(expected_answer_count_, rv);
      completed_ = true;
    }
  }

  void Cancel() {
    ASSERT_TRUE(transaction_.get() != NULL);
    transaction_.reset(NULL);
  }

  void OnTransactionComplete(DnsTransaction* t,
                             int rv,
                             const DnsResponse* response) {
    EXPECT_FALSE(completed_);
    EXPECT_EQ(transaction_.get(), t);

    completed_ = true;

    if (cancel_in_callback_) {
      Cancel();
      return;
    }

    if (expected_answer_count_ >= 0) {
      EXPECT_EQ(OK, rv);
      EXPECT_EQ(static_cast<unsigned>(expected_answer_count_),
                response->answer_count());
      EXPECT_EQ(qtype_, response->qtype());

      DnsRecordParser parser = response->Parser();
      DnsResourceRecord record;
      for (int i = 0; i < expected_answer_count_; ++i) {
        EXPECT_TRUE(parser.ReadRecord(&record));
      }
    } else {
      EXPECT_EQ(expected_answer_count_, rv);
    }

    if (quit_in_callback_)
      MessageLoop::current()->Quit();
  }

  bool has_completed() const {
    return completed_;
  }

  // Shorthands for commonly used commands.

  bool Run(DnsTransactionFactory* factory) {
    StartTransaction(factory);
    MessageLoop::current()->RunAllPending();
    return has_completed();
  }

  // Use when some of the responses are timeouts.
  bool RunUntilDone(DnsTransactionFactory* factory) {
    set_quit_in_callback();
    StartTransaction(factory);
    MessageLoop::current()->Run();
    return has_completed();
  }

 private:
  std::string hostname_;
  uint16 qtype_;
  scoped_ptr<DnsTransaction> transaction_;
  int expected_answer_count_;
  bool cancel_in_callback_;
  bool quit_in_callback_;

  bool completed_;
};

class DnsTransactionTest : public testing::Test {
 public:
  DnsTransactionTest() : socket_factory_(NULL) {}

  // Generates |nameservers| for DnsConfig.
  void ConfigureNumServers(unsigned num_servers) {
    CHECK_LE(num_servers, 255u);
    config_.nameservers.clear();
    IPAddressNumber dns_ip;
    {
      bool rv = ParseIPLiteralToNumber("192.168.1.0", &dns_ip);
      EXPECT_TRUE(rv);
    }
    for (unsigned i = 0; i < num_servers; ++i) {
      dns_ip[3] = i;
      config_.nameservers.push_back(IPEndPoint(dns_ip,
                                               dns_protocol::kDefaultPort));
    }
  }

  // Called after fully configuring |config|.
  void ConfigureFactory() {
    socket_factory_.reset(new TestSocketFactory());
    session_ = new DnsSession(
        config_,
        socket_factory_.get(),
        base::Bind(&DnsTransactionTest::GetNextId, base::Unretained(this)),
        NULL /* NetLog */);
    transaction_factory_ = DnsTransactionFactory::CreateFactory(session_.get());
  }

  // Each socket used by a DnsTransaction expects only one write and zero or one
  // reads.

  // Add expected query for |dotted_name| and |qtype| with |id| and response
  // taken verbatim from |data| of |data_length| bytes. The transaction id in
  // |data| should equal |id|, unless testing mismatched response.
  void AddResponse(const std::string& dotted_name,
                   uint16 qtype,
                   uint16 id,
                   const char* data,
                   size_t data_length,
                   IoMode mode) {
    CHECK(socket_factory_.get());
    DnsQuery* query = new DnsQuery(id, DomainFromDot(dotted_name), qtype);
    queries_.push_back(query);

    // The response is only used to hold the IOBuffer.
    DnsResponse* response = new DnsResponse(data, data_length, 0);
    responses_.push_back(response);

    writes_.push_back(MockWrite(mode,
                                query->io_buffer()->data(),
                                query->io_buffer()->size()));
    reads_.push_back(MockRead(mode,
                              response->io_buffer()->data(),
                              data_length));

    transaction_ids_.push_back(id);
  }

  void AddAsyncResponse(const std::string& dotted_name,
                        uint16 qtype,
                        uint16 id,
                        const char* data,
                        size_t data_length) {
    AddResponse(dotted_name, qtype, id, data, data_length, ASYNC);
  }

  // Add expected query of |dotted_name| and |qtype| and no response.
  void AddTimeout(const char* dotted_name, uint16 qtype) {
    CHECK(socket_factory_.get());
    uint16 id = base::RandInt(0, kuint16max);
    DnsQuery* query = new DnsQuery(id, DomainFromDot(dotted_name), qtype);
    queries_.push_back(query);

    writes_.push_back(MockWrite(ASYNC,
                                query->io_buffer()->data(),
                                query->io_buffer()->size()));
    // Empty MockRead indicates no data.
    reads_.push_back(MockRead());
    transaction_ids_.push_back(id);
  }

  // Add expected query of |dotted_name| and |qtype| and response with no answer
  // and rcode set to |rcode|.
  void AddRcode(const char* dotted_name, uint16 qtype, int rcode, IoMode mode) {
    CHECK(socket_factory_.get());
    CHECK_NE(dns_protocol::kRcodeNOERROR, rcode);
    uint16 id = base::RandInt(0, kuint16max);
    DnsQuery* query = new DnsQuery(id, DomainFromDot(dotted_name), qtype);
    queries_.push_back(query);

    DnsResponse* response = new DnsResponse(query->io_buffer()->data(),
                                            query->io_buffer()->size(),
                                            0);
    dns_protocol::Header* header =
        reinterpret_cast<dns_protocol::Header*>(response->io_buffer()->data());
    header->flags |= base::HostToNet16(dns_protocol::kFlagResponse | rcode);
    responses_.push_back(response);

    writes_.push_back(MockWrite(mode,
                                query->io_buffer()->data(),
                                query->io_buffer()->size()));
    reads_.push_back(MockRead(mode,
                              response->io_buffer()->data(),
                              query->io_buffer()->size()));
    transaction_ids_.push_back(id);
  }

  void AddAsyncRcode(const char* dotted_name, uint16 qtype, int rcode) {
    AddRcode(dotted_name, qtype, rcode, ASYNC);
  }

  // Call after all Add* calls to prepare data for |socket_factory_|.
  // This separation is necessary because the |reads_| and |writes_| vectors
  // could reallocate their data during those calls.
  void PrepareSockets() {
    CHECK_EQ(writes_.size(), reads_.size());
    for (size_t i = 0; i < writes_.size(); ++i) {
      DelayedSocketData* data;
      if (reads_[i].data) {
        data = new DelayedSocketData(1, &reads_[i], 1, &writes_[i], 1);
      } else {
        // Timeout.
        data = new DelayedSocketData(2, NULL, 0, &writes_[i], 1);
      }
      socket_data_.push_back(data);
      socket_factory_->AddSocketDataProvider(data);
    }
  }

  // Checks if the sockets were connected in the order matching the indices in
  // |servers|.
  void CheckServerOrder(const unsigned* servers, size_t num_attempts) {
    ASSERT_EQ(num_attempts, socket_factory_->remote_endpoints.size());
    for (size_t i = 0; i < num_attempts; ++i) {
      EXPECT_EQ(socket_factory_->remote_endpoints[i],
                session_->config().nameservers[servers[i]]);
    }
  }

  void SetUp() OVERRIDE {
    // By default set one server,
    ConfigureNumServers(1);
    // and no retransmissions,
    config_.attempts = 1;
    // but long enough timeout for memory tests.
    config_.timeout = TestTimeouts::action_timeout();
    ConfigureFactory();
  }

  void TearDown() OVERRIDE {
    // Check that all socket data was at least written to.
    for (size_t i = 0; i < socket_data_.size(); ++i) {
      EXPECT_GT(socket_data_[i]->write_index(), 0u);
    }
  }

 protected:
  int GetNextId(int min, int max) {
    EXPECT_FALSE(transaction_ids_.empty());
    int id = transaction_ids_.front();
    transaction_ids_.pop_front();
    EXPECT_GE(id, min);
    EXPECT_LE(id, max);
    return id;
  }

  DnsConfig config_;

  // Holders for the buffers behind MockRead/MockWrites (they do not own them).
  ScopedVector<DnsQuery> queries_;
  ScopedVector<DnsResponse> responses_;

  // Holders for MockRead/MockWrites (SocketDataProvider does not own it).
  std::vector<MockRead> reads_;
  std::vector<MockWrite> writes_;

  // Holder for the socket data (MockClientSocketFactory does not own it).
  ScopedVector<DelayedSocketData> socket_data_;

  std::deque<int> transaction_ids_;
  scoped_ptr<TestSocketFactory> socket_factory_;
  scoped_refptr<DnsSession> session_;
  scoped_ptr<DnsTransactionFactory> transaction_factory_;
};

TEST_F(DnsTransactionTest, Lookup) {
  AddAsyncResponse(kT0HostName,
                   kT0Qtype,
                   0 /* id */,
                   reinterpret_cast<const char*>(kT0ResponseDatagram),
                   arraysize(kT0ResponseDatagram));
  PrepareSockets();

  TransactionHelper helper0(kT0HostName,
                            kT0Qtype,
                            kT0RecordCount);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

// Concurrent lookup tests assume that DnsTransaction::Start immediately
// consumes a socket from ClientSocketFactory.
TEST_F(DnsTransactionTest, ConcurrentLookup) {
  AddAsyncResponse(kT0HostName,
                   kT0Qtype,
                   0 /* id */,
                   reinterpret_cast<const char*>(kT0ResponseDatagram),
                   arraysize(kT0ResponseDatagram));
  AddAsyncResponse(kT1HostName,
                   kT1Qtype,
                   1 /* id */,
                   reinterpret_cast<const char*>(kT1ResponseDatagram),
                   arraysize(kT1ResponseDatagram));
  PrepareSockets();

  TransactionHelper helper0(kT0HostName,
                            kT0Qtype,
                            kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get());
  TransactionHelper helper1(kT1HostName,
                            kT1Qtype,
                            kT1RecordCount);
  helper1.StartTransaction(transaction_factory_.get());

  MessageLoop::current()->RunAllPending();

  EXPECT_TRUE(helper0.has_completed());
  EXPECT_TRUE(helper1.has_completed());
}

TEST_F(DnsTransactionTest, CancelLookup) {
  AddAsyncResponse(kT0HostName,
                   kT0Qtype,
                   0 /* id */,
                   reinterpret_cast<const char*>(kT0ResponseDatagram),
                   arraysize(kT0ResponseDatagram));
  AddAsyncResponse(kT1HostName,
                   kT1Qtype,
                   1 /* id */,
                   reinterpret_cast<const char*>(kT1ResponseDatagram),
                   arraysize(kT1ResponseDatagram));
  PrepareSockets();

  TransactionHelper helper0(kT0HostName,
                            kT0Qtype,
                            kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get());
  TransactionHelper helper1(kT1HostName,
                            kT1Qtype,
                            kT1RecordCount);
  helper1.StartTransaction(transaction_factory_.get());

  helper0.Cancel();

  MessageLoop::current()->RunAllPending();

  EXPECT_FALSE(helper0.has_completed());
  EXPECT_TRUE(helper1.has_completed());
}

TEST_F(DnsTransactionTest, DestroyFactory) {
  AddAsyncResponse(kT0HostName,
                   kT0Qtype,
                   0 /* id */,
                   reinterpret_cast<const char*>(kT0ResponseDatagram),
                   arraysize(kT0ResponseDatagram));
  PrepareSockets();

  TransactionHelper helper0(kT0HostName,
                            kT0Qtype,
                            kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get());

  // Destroying the client does not affect running requests.
  transaction_factory_.reset(NULL);

  MessageLoop::current()->RunAllPending();

  EXPECT_TRUE(helper0.has_completed());
}

TEST_F(DnsTransactionTest, CancelFromCallback) {
  AddAsyncResponse(kT0HostName,
                   kT0Qtype,
                   0 /* id */,
                   reinterpret_cast<const char*>(kT0ResponseDatagram),
                   arraysize(kT0ResponseDatagram));
  PrepareSockets();

  TransactionHelper helper0(kT0HostName,
                            kT0Qtype,
                            kT0RecordCount);
  helper0.set_cancel_in_callback();
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, ServerFail) {
  AddAsyncRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL);
  PrepareSockets();

  TransactionHelper helper0(kT0HostName,
                            kT0Qtype,
                            ERR_DNS_SERVER_FAILED);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, NoDomain) {
  AddAsyncRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeNXDOMAIN);
  PrepareSockets();

  TransactionHelper helper0(kT0HostName,
                            kT0Qtype,
                            ERR_NAME_NOT_RESOLVED);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, Timeout) {
  config_.attempts = 3;
  // Use short timeout to speed up the test.
  config_.timeout = base::TimeDelta::FromMilliseconds(
      TestTimeouts::tiny_timeout_ms());
  ConfigureFactory();

  AddTimeout(kT0HostName, kT0Qtype);
  AddTimeout(kT0HostName, kT0Qtype);
  AddTimeout(kT0HostName, kT0Qtype);
  PrepareSockets();

  TransactionHelper helper0(kT0HostName,
                            kT0Qtype,
                            ERR_DNS_TIMED_OUT);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
  MessageLoop::current()->AssertIdle();
}

TEST_F(DnsTransactionTest, ServerFallbackAndRotate) {
  // Test that we fallback on both server failure and timeout.
  config_.attempts = 2;
  // The next request should start from the next server.
  config_.rotate = true;
  ConfigureNumServers(3);
  // Use short timeout to speed up the test.
  config_.timeout = base::TimeDelta::FromMilliseconds(
      TestTimeouts::tiny_timeout_ms());
  ConfigureFactory();

  // Responses for first request.
  AddTimeout(kT0HostName, kT0Qtype);
  AddAsyncRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL);
  AddTimeout(kT0HostName, kT0Qtype);
  AddAsyncRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL);
  AddAsyncRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeNXDOMAIN);
  // Responses for second request.
  AddAsyncRcode(kT1HostName, kT1Qtype, dns_protocol::kRcodeSERVFAIL);
  AddAsyncRcode(kT1HostName, kT1Qtype, dns_protocol::kRcodeNXDOMAIN);
  PrepareSockets();

  TransactionHelper helper0(kT0HostName,
                            kT0Qtype,
                            ERR_NAME_NOT_RESOLVED);
  TransactionHelper helper1(kT1HostName,
                            kT1Qtype,
                            ERR_NAME_NOT_RESOLVED);

  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
  EXPECT_TRUE(helper1.Run(transaction_factory_.get()));

  unsigned kOrder[] = {
      0, 1, 2, 0, 1,    // The first transaction.
      1, 2,             // The second transaction starts from the next server.
  };
  CheckServerOrder(kOrder, arraysize(kOrder));
}

TEST_F(DnsTransactionTest, SuffixSearchAboveNdots) {
  config_.ndots = 2;
  config_.search.push_back("a");
  config_.search.push_back("b");
  config_.search.push_back("c");
  config_.rotate = true;
  ConfigureNumServers(2);
  ConfigureFactory();

  AddAsyncRcode("x.y.z", dns_protocol::kTypeA, dns_protocol::kRcodeNXDOMAIN);
  AddAsyncRcode("x.y.z.a", dns_protocol::kTypeA, dns_protocol::kRcodeNXDOMAIN);
  AddAsyncRcode("x.y.z.b", dns_protocol::kTypeA, dns_protocol::kRcodeNXDOMAIN);
  AddAsyncRcode("x.y.z.c", dns_protocol::kTypeA, dns_protocol::kRcodeNXDOMAIN);
  PrepareSockets();

  TransactionHelper helper0("x.y.z",
                            dns_protocol::kTypeA,
                            ERR_NAME_NOT_RESOLVED);

  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));

  // Also check if suffix search causes server rotation.
  unsigned kOrder0[] = { 0, 1, 0, 1 };
  CheckServerOrder(kOrder0, arraysize(kOrder0));
}

TEST_F(DnsTransactionTest, SuffixSearchBelowNdots) {
  config_.ndots = 2;
  config_.search.push_back("a");
  config_.search.push_back("b");
  config_.search.push_back("c");
  ConfigureFactory();

  // Responses for first transaction.
  AddAsyncRcode("x.y.a", dns_protocol::kTypeA, dns_protocol::kRcodeNXDOMAIN);
  AddAsyncRcode("x.y.b", dns_protocol::kTypeA, dns_protocol::kRcodeNXDOMAIN);
  AddAsyncRcode("x.y.c", dns_protocol::kTypeA, dns_protocol::kRcodeNXDOMAIN);
  AddAsyncRcode("x.y", dns_protocol::kTypeA, dns_protocol::kRcodeNXDOMAIN);
  // Responses for second transaction.
  AddAsyncRcode("x.a", dns_protocol::kTypeA, dns_protocol::kRcodeNXDOMAIN);
  AddAsyncRcode("x.b", dns_protocol::kTypeA, dns_protocol::kRcodeNXDOMAIN);
  AddAsyncRcode("x.c", dns_protocol::kTypeA, dns_protocol::kRcodeNXDOMAIN);
  // Responses for third transaction.
  AddAsyncRcode("x", dns_protocol::kTypeAAAA, dns_protocol::kRcodeNXDOMAIN);
  PrepareSockets();

  TransactionHelper helper0("x.y",
                            dns_protocol::kTypeA,
                            ERR_NAME_NOT_RESOLVED);

  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));

  // A single-label name.
  TransactionHelper helper1("x",
                            dns_protocol::kTypeA,
                            ERR_NAME_NOT_RESOLVED);

  EXPECT_TRUE(helper1.Run(transaction_factory_.get()));

  // A fully-qualified name.
  TransactionHelper helper2("x.",
                            dns_protocol::kTypeAAAA,
                            ERR_NAME_NOT_RESOLVED);

  EXPECT_TRUE(helper2.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, EmptySuffixSearch) {
  // Responses for first transaction.
  AddAsyncRcode("x", dns_protocol::kTypeA, dns_protocol::kRcodeNXDOMAIN);
  PrepareSockets();

  // A fully-qualified name.
  TransactionHelper helper0("x.",
                            dns_protocol::kTypeA,
                            ERR_NAME_NOT_RESOLVED);

  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));

  // A single label name is not even attempted.
  TransactionHelper helper1("singlelabel",
                            dns_protocol::kTypeA,
                            ERR_NAME_NOT_RESOLVED);

  helper1.StartTransaction(transaction_factory_.get());
  EXPECT_TRUE(helper1.has_completed());
}

TEST_F(DnsTransactionTest, DontAppendToMultiLabelName) {
  config_.search.push_back("a");
  config_.search.push_back("b");
  config_.search.push_back("c");
  config_.append_to_multi_label_name = false;
  ConfigureFactory();

  // Responses for first transaction.
  AddAsyncRcode("x.y.z", dns_protocol::kTypeA, dns_protocol::kRcodeNXDOMAIN);
  // Responses for second transaction.
  AddAsyncRcode("x.y", dns_protocol::kTypeA, dns_protocol::kRcodeNXDOMAIN);
  // Responses for third transaction.
  AddAsyncRcode("x.a", dns_protocol::kTypeA, dns_protocol::kRcodeNXDOMAIN);
  AddAsyncRcode("x.b", dns_protocol::kTypeA, dns_protocol::kRcodeNXDOMAIN);
  AddAsyncRcode("x.c", dns_protocol::kTypeA, dns_protocol::kRcodeNXDOMAIN);
  PrepareSockets();

  TransactionHelper helper0("x.y.z",
                            dns_protocol::kTypeA,
                            ERR_NAME_NOT_RESOLVED);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));

  TransactionHelper helper1("x.y",
                            dns_protocol::kTypeA,
                            ERR_NAME_NOT_RESOLVED);
  EXPECT_TRUE(helper1.Run(transaction_factory_.get()));

  TransactionHelper helper2("x",
                            dns_protocol::kTypeA,
                            ERR_NAME_NOT_RESOLVED);
  EXPECT_TRUE(helper2.Run(transaction_factory_.get()));
}

const uint8 kResponseNoData[] = {
  0x00, 0x00, 0x81, 0x80, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
  // Question
  0x01,  'x', 0x01,  'y', 0x01,  'z', 0x01,  'b', 0x00, 0x00, 0x01, 0x00, 0x01,
  // Authority section, SOA record, TTL 0x3E6
  0x01,  'z', 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x00, 0x03, 0xE6,
  // Minimal RDATA, 18 bytes
  0x00, 0x12,
  0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
};

TEST_F(DnsTransactionTest, SuffixSearchStop) {
  config_.ndots = 2;
  config_.search.push_back("a");
  config_.search.push_back("b");
  config_.search.push_back("c");
  ConfigureFactory();

  AddAsyncRcode("x.y.z", dns_protocol::kTypeA, dns_protocol::kRcodeNXDOMAIN);
  AddAsyncRcode("x.y.z.a", dns_protocol::kTypeA, dns_protocol::kRcodeNXDOMAIN);
  AddAsyncResponse("x.y.z.b",
                   dns_protocol::kTypeA,
                   0 /* id */,
                   reinterpret_cast<const char*>(kResponseNoData),
                   arraysize(kResponseNoData));
  PrepareSockets();

  TransactionHelper helper0("x.y.z", dns_protocol::kTypeA, 0 /* answers */);

  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, SyncFirstQuery) {
  config_.search.push_back("lab.ccs.neu.edu");
  config_.search.push_back("ccs.neu.edu");
  ConfigureFactory();

  AddResponse(kT0HostName,
              kT0Qtype,
              0 /* id */,
              reinterpret_cast<const char*>(kT0ResponseDatagram),
              arraysize(kT0ResponseDatagram),
              SYNCHRONOUS);
  PrepareSockets();

  TransactionHelper helper0(kT0HostName,
                            kT0Qtype,
                            kT0RecordCount);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, SyncFirstQueryWithSearch) {
  config_.search.push_back("lab.ccs.neu.edu");
  config_.search.push_back("ccs.neu.edu");
  ConfigureFactory();

  AddRcode("www.lab.ccs.neu.edu",
           kT2Qtype,
           dns_protocol::kRcodeNXDOMAIN,
           SYNCHRONOUS);
  AddResponse(kT2HostName,  // "www.ccs.neu.edu"
              kT2Qtype,
              2 /* id */,
              reinterpret_cast<const char*>(kT2ResponseDatagram),
              arraysize(kT2ResponseDatagram),
              ASYNC);
  PrepareSockets();

  TransactionHelper helper0("www",
                            kT2Qtype,
                            kT2RecordCount);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, SyncSearchQuery) {
  config_.search.push_back("lab.ccs.neu.edu");
  config_.search.push_back("ccs.neu.edu");
  ConfigureFactory();

  AddRcode("www.lab.ccs.neu.edu",
           dns_protocol::kTypeA,
           dns_protocol::kRcodeNXDOMAIN,
           ASYNC);
  AddResponse(kT2HostName,
              kT2Qtype,
              2 /* id */,
              reinterpret_cast<const char*>(kT2ResponseDatagram),
              arraysize(kT2ResponseDatagram),
              SYNCHRONOUS);
  PrepareSockets();

  TransactionHelper helper0("www",
                            kT2Qtype,
                            kT2RecordCount);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

}  // namespace

}  // namespace net
