// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_transaction.h"

#include <deque>
#include <vector>

#include "base/bind.h"
#include "base/test/test_timeouts.h"
#include "base/time.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_session.h"
#include "net/dns/dns_test_util.h"
#include "net/socket/socket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// A mock for RandIntCallback that always returns 0.
int ReturnZero(int min, int max) {
  return 0;
}

class DnsTransactionTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    callback_ = base::Bind(&DnsTransactionTest::OnTransactionComplete,
                           base::Unretained(this));
    qname_ = std::string(kT0DnsName, arraysize(kT0DnsName));
    // Use long timeout to prevent timing out on slow bots.
    ConfigureSession(base::TimeDelta::FromMilliseconds(
        TestTimeouts::action_timeout_ms()));
  }

  void ConfigureSession(const base::TimeDelta& timeout) {
    IPEndPoint dns_server;
    bool rv = CreateDnsAddress(kDnsIp, kDnsPort, &dns_server);
    ASSERT_TRUE(rv);

    DnsConfig config;
    config.nameservers.push_back(dns_server);
    config.attempts = 3;
    config.timeout = timeout;

    session_ = new DnsSession(config,
                              new MockClientSocketFactory(),
                              base::Bind(&ReturnZero),
                              NULL /* NetLog */);
  }

  void StartTransaction() {
    transaction_.reset(new DnsTransaction(session_.get(),
                                          qname_,
                                          kT0Qtype,
                                          callback_,
                                          BoundNetLog()));

    int rv0 = transaction_->Start();
    EXPECT_EQ(ERR_IO_PENDING, rv0);
  }

  void OnTransactionComplete(DnsTransaction* transaction, int rv) {
    EXPECT_EQ(transaction_.get(), transaction);
    EXPECT_EQ(qname_, transaction->query()->qname().as_string());
    EXPECT_EQ(kT0Qtype, transaction->query()->qtype());
    rv_ = rv;
    MessageLoop::current()->Quit();
  }

  MockClientSocketFactory& factory() {
    return *static_cast<MockClientSocketFactory*>(session_->socket_factory());
  }

  int rv() const { return rv_; }

 private:
  DnsTransaction::ResultCallback callback_;
  std::string qname_;
  scoped_refptr<DnsSession> session_;
  scoped_ptr<DnsTransaction> transaction_;

  int rv_;
};

TEST_F(DnsTransactionTest, NormalQueryResponseTest) {
  MockWrite writes0[] = {
    MockWrite(true, reinterpret_cast<const char*>(kT0QueryDatagram),
              arraysize(kT0QueryDatagram))
  };

  MockRead reads0[] = {
    MockRead(true, reinterpret_cast<const char*>(kT0ResponseDatagram),
             arraysize(kT0ResponseDatagram))
  };

  StaticSocketDataProvider data(reads0, arraysize(reads0),
                                writes0, arraysize(writes0));
  factory().AddSocketDataProvider(&data);

  StartTransaction();
  MessageLoop::current()->Run();

  EXPECT_EQ(OK, rv());
  // TODO(szym): test fields of |transaction_->response()|

  EXPECT_TRUE(data.at_read_eof());
  EXPECT_TRUE(data.at_write_eof());
}

TEST_F(DnsTransactionTest, MismatchedQueryResponseTest) {
  MockWrite writes0[] = {
    MockWrite(true, reinterpret_cast<const char*>(kT0QueryDatagram),
              arraysize(kT0QueryDatagram))
  };

  MockRead reads1[] = {
    MockRead(true, reinterpret_cast<const char*>(kT1ResponseDatagram),
             arraysize(kT1ResponseDatagram))
  };

  StaticSocketDataProvider data(reads1, arraysize(reads1),
                                writes0, arraysize(writes0));
  factory().AddSocketDataProvider(&data);

  StartTransaction();
  MessageLoop::current()->Run();

  EXPECT_EQ(ERR_DNS_MALFORMED_RESPONSE, rv());

  EXPECT_TRUE(data.at_read_eof());
  EXPECT_TRUE(data.at_write_eof());
}

// Test that after the first timeout we do a fresh connection and if we get
// a response on the new connection, we return it.
TEST_F(DnsTransactionTest, FirstTimeoutTest) {
  MockWrite writes0[] = {
    MockWrite(true, reinterpret_cast<const char*>(kT0QueryDatagram),
              arraysize(kT0QueryDatagram))
  };

  MockRead reads0[] = {
    MockRead(true, reinterpret_cast<const char*>(kT0ResponseDatagram),
             arraysize(kT0ResponseDatagram))
  };

  scoped_refptr<DelayedSocketData> socket0_data(
      new DelayedSocketData(2, NULL, 0, writes0, arraysize(writes0)));
  scoped_refptr<DelayedSocketData> socket1_data(
      new DelayedSocketData(0, reads0, arraysize(reads0),
                            writes0, arraysize(writes0)));

  // Use short timeout to speed up the test.
  ConfigureSession(base::TimeDelta::FromMilliseconds(
      TestTimeouts::tiny_timeout_ms()));
  factory().AddSocketDataProvider(socket0_data.get());
  factory().AddSocketDataProvider(socket1_data.get());

  StartTransaction();

  MessageLoop::current()->Run();

  EXPECT_EQ(OK, rv());

  EXPECT_TRUE(socket0_data->at_read_eof());
  EXPECT_TRUE(socket0_data->at_write_eof());
  EXPECT_TRUE(socket1_data->at_read_eof());
  EXPECT_TRUE(socket1_data->at_write_eof());
  EXPECT_EQ(2u, factory().udp_client_sockets().size());
}

// Test that after the first timeout we do a fresh connection, and after
// the second timeout we do another fresh connection, and if we get a
// response on the second connection, we return it.
TEST_F(DnsTransactionTest, SecondTimeoutTest) {
  MockWrite writes0[] = {
    MockWrite(true, reinterpret_cast<const char*>(kT0QueryDatagram),
              arraysize(kT0QueryDatagram))
  };

  MockRead reads0[] = {
    MockRead(true, reinterpret_cast<const char*>(kT0ResponseDatagram),
             arraysize(kT0ResponseDatagram))
  };

  scoped_refptr<DelayedSocketData> socket0_data(
      new DelayedSocketData(2, NULL, 0, writes0, arraysize(writes0)));
  scoped_refptr<DelayedSocketData> socket1_data(
      new DelayedSocketData(2, NULL, 0, writes0, arraysize(writes0)));
  scoped_refptr<DelayedSocketData> socket2_data(
      new DelayedSocketData(0, reads0, arraysize(reads0),
                            writes0, arraysize(writes0)));

  // Use short timeout to speed up the test.
  ConfigureSession(base::TimeDelta::FromMilliseconds(
      TestTimeouts::tiny_timeout_ms()));
  factory().AddSocketDataProvider(socket0_data.get());
  factory().AddSocketDataProvider(socket1_data.get());
  factory().AddSocketDataProvider(socket2_data.get());

  StartTransaction();

  MessageLoop::current()->Run();

  EXPECT_EQ(OK, rv());

  EXPECT_TRUE(socket0_data->at_read_eof());
  EXPECT_TRUE(socket0_data->at_write_eof());
  EXPECT_TRUE(socket1_data->at_read_eof());
  EXPECT_TRUE(socket1_data->at_write_eof());
  EXPECT_TRUE(socket2_data->at_read_eof());
  EXPECT_TRUE(socket2_data->at_write_eof());
  EXPECT_EQ(3u, factory().udp_client_sockets().size());
}

// Test that after the first timeout we do a fresh connection, and after
// the second timeout we do another fresh connection and after the third
// timeout we give up and return a timeout error.
TEST_F(DnsTransactionTest, ThirdTimeoutTest) {
  MockWrite writes0[] = {
    MockWrite(true, reinterpret_cast<const char*>(kT0QueryDatagram),
              arraysize(kT0QueryDatagram))
  };

  scoped_refptr<DelayedSocketData> socket0_data(
      new DelayedSocketData(2, NULL, 0, writes0, arraysize(writes0)));
  scoped_refptr<DelayedSocketData> socket1_data(
      new DelayedSocketData(2, NULL, 0, writes0, arraysize(writes0)));
  scoped_refptr<DelayedSocketData> socket2_data(
      new DelayedSocketData(2, NULL, 0, writes0, arraysize(writes0)));

  // Use short timeout to speed up the test.
  ConfigureSession(base::TimeDelta::FromMilliseconds(
      TestTimeouts::tiny_timeout_ms()));
  factory().AddSocketDataProvider(socket0_data.get());
  factory().AddSocketDataProvider(socket1_data.get());
  factory().AddSocketDataProvider(socket2_data.get());

  StartTransaction();

  MessageLoop::current()->Run();

  EXPECT_EQ(ERR_DNS_TIMED_OUT, rv());

  EXPECT_TRUE(socket0_data->at_read_eof());
  EXPECT_TRUE(socket0_data->at_write_eof());
  EXPECT_TRUE(socket1_data->at_read_eof());
  EXPECT_TRUE(socket1_data->at_write_eof());
  EXPECT_TRUE(socket2_data->at_read_eof());
  EXPECT_TRUE(socket2_data->at_write_eof());
  EXPECT_EQ(3u, factory().udp_client_sockets().size());
}

}  // namespace

}  // namespace net
