// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_transaction.h"

#include <deque>
#include <vector>

#include "base/bind.h"
#include "net/dns/dns_test_util.h"
#include "net/socket/socket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

static const base::TimeDelta kTimeoutsMs[] = {
  base::TimeDelta::FromMilliseconds(20),
  base::TimeDelta::FromMilliseconds(20),
  base::TimeDelta::FromMilliseconds(20),
};

}  // namespace

class TestDelegate : public DnsTransaction::Delegate {
 public:
  TestDelegate() : result_(ERR_UNEXPECTED), transaction_(NULL) {}
  virtual ~TestDelegate() {}
  virtual void OnTransactionComplete(
      int result,
      const DnsTransaction* transaction,
      const IPAddressList& ip_addresses) {
    result_ = result;
    transaction_ = transaction;
    ip_addresses_ = ip_addresses;
    MessageLoop::current()->Quit();
  }
  int result() const { return result_; }
  const DnsTransaction* transaction() const { return transaction_; }
  const IPAddressList& ip_addresses() const {
    return ip_addresses_;
  }

 private:
  int result_;
  const DnsTransaction* transaction_;
  IPAddressList ip_addresses_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};


TEST(DnsTransactionTest, NormalQueryResponseTest) {
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
  MockClientSocketFactory factory;
  factory.AddSocketDataProvider(&data);

  TestPrng test_prng(std::deque<int>(1, 0));
  RandIntCallback rand_int_cb =
      base::Bind(&TestPrng::GetNext, base::Unretained(&test_prng));
  std::string t0_dns_name(kT0DnsName, arraysize(kT0DnsName));

  IPEndPoint dns_server;
  bool rv = CreateDnsAddress(kDnsIp, kDnsPort, &dns_server);
  ASSERT_TRUE(rv);

  DnsTransaction t(dns_server, t0_dns_name, kT1Qtype, rand_int_cb, &factory,
                   BoundNetLog(), NULL);

  TestDelegate delegate;
  t.SetDelegate(&delegate);

  IPAddressList expected_ip_addresses;
  rv = ConvertStringsToIPAddressList(kT0IpAddresses,
                                     arraysize(kT0IpAddresses),
                                     &expected_ip_addresses);
  ASSERT_TRUE(rv);

  int rv0 = t.Start();
  EXPECT_EQ(ERR_IO_PENDING, rv0);

  MessageLoop::current()->Run();

  EXPECT_TRUE(DnsTransaction::Key(t0_dns_name, kT0Qtype) == t.key());
  EXPECT_EQ(OK, delegate.result());
  EXPECT_EQ(&t, delegate.transaction());
  EXPECT_TRUE(expected_ip_addresses == delegate.ip_addresses());

  EXPECT_TRUE(data.at_read_eof());
  EXPECT_TRUE(data.at_write_eof());
}

TEST(DnsTransactionTest, MismatchedQueryResponseTest) {
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
  MockClientSocketFactory factory;
  factory.AddSocketDataProvider(&data);

  TestPrng test_prng(std::deque<int>(1, 0));
  RandIntCallback rand_int_cb =
      base::Bind(&TestPrng::GetNext, base::Unretained(&test_prng));
  std::string t0_dns_name(kT0DnsName, arraysize(kT0DnsName));

  IPEndPoint dns_server;
  bool rv = CreateDnsAddress(kDnsIp, kDnsPort, &dns_server);
  ASSERT_TRUE(rv);

  DnsTransaction t(dns_server, t0_dns_name, kT1Qtype, rand_int_cb, &factory,
                   BoundNetLog(), NULL);

  TestDelegate delegate;
  t.SetDelegate(&delegate);

  int rv0 = t.Start();
  EXPECT_EQ(ERR_IO_PENDING, rv0);

  MessageLoop::current()->Run();

  EXPECT_TRUE(DnsTransaction::Key(t0_dns_name, kT0Qtype) == t.key());
  EXPECT_EQ(ERR_DNS_MALFORMED_RESPONSE, delegate.result());
  EXPECT_EQ(0u, delegate.ip_addresses().size());
  EXPECT_EQ(&t, delegate.transaction());
  EXPECT_TRUE(data.at_read_eof());
  EXPECT_TRUE(data.at_write_eof());
}

// Test that after the first timeout we do a fresh connection and if we get
// a response on the new connection, we return it.
TEST(DnsTransactionTest, FirstTimeoutTest) {
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
  MockClientSocketFactory factory;
  factory.AddSocketDataProvider(socket0_data.get());
  factory.AddSocketDataProvider(socket1_data.get());

  TestPrng test_prng(std::deque<int>(2, 0));
  RandIntCallback rand_int_cb =
      base::Bind(&TestPrng::GetNext, base::Unretained(&test_prng));
  std::string t0_dns_name(kT0DnsName, arraysize(kT0DnsName));

  IPEndPoint dns_server;
  bool rv = CreateDnsAddress(kDnsIp, kDnsPort, &dns_server);
  ASSERT_TRUE(rv);

  DnsTransaction t(dns_server, t0_dns_name, kT1Qtype, rand_int_cb, &factory,
                   BoundNetLog(), NULL);

  TestDelegate delegate;
  t.SetDelegate(&delegate);

  t.set_timeouts_ms(
      std::vector<base::TimeDelta>(kTimeoutsMs,
                                   kTimeoutsMs + arraysize(kTimeoutsMs)));

  IPAddressList expected_ip_addresses;
  rv = ConvertStringsToIPAddressList(kT0IpAddresses,
                                     arraysize(kT0IpAddresses),
                                     &expected_ip_addresses);
  ASSERT_TRUE(rv);

  int rv0 = t.Start();
  EXPECT_EQ(ERR_IO_PENDING, rv0);


  MessageLoop::current()->Run();

  EXPECT_TRUE(DnsTransaction::Key(t0_dns_name, kT0Qtype) == t.key());
  EXPECT_EQ(OK, delegate.result());
  EXPECT_EQ(&t, delegate.transaction());
  EXPECT_TRUE(expected_ip_addresses == delegate.ip_addresses());

  EXPECT_TRUE(socket0_data->at_read_eof());
  EXPECT_TRUE(socket0_data->at_write_eof());
  EXPECT_TRUE(socket1_data->at_read_eof());
  EXPECT_TRUE(socket1_data->at_write_eof());
  EXPECT_EQ(2u, factory.udp_client_sockets().size());
}

// Test that after the first timeout we do a fresh connection, and after
// the second timeout we do another fresh connection, and if we get a
// response on the second connection, we return it.
TEST(DnsTransactionTest, SecondTimeoutTest) {
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
  MockClientSocketFactory factory;
  factory.AddSocketDataProvider(socket0_data.get());
  factory.AddSocketDataProvider(socket1_data.get());
  factory.AddSocketDataProvider(socket2_data.get());

  TestPrng test_prng(std::deque<int>(3, 0));
  RandIntCallback rand_int_cb =
      base::Bind(&TestPrng::GetNext, base::Unretained(&test_prng));
  std::string t0_dns_name(kT0DnsName, arraysize(kT0DnsName));

  IPEndPoint dns_server;
  bool rv = CreateDnsAddress(kDnsIp, kDnsPort, &dns_server);
  ASSERT_TRUE(rv);

  DnsTransaction t(dns_server, t0_dns_name, kT1Qtype, rand_int_cb, &factory,
                   BoundNetLog(), NULL);

  TestDelegate delegate;
  t.SetDelegate(&delegate);

  t.set_timeouts_ms(
      std::vector<base::TimeDelta>(kTimeoutsMs,
                                   kTimeoutsMs + arraysize(kTimeoutsMs)));

  IPAddressList expected_ip_addresses;
  rv = ConvertStringsToIPAddressList(kT0IpAddresses,
                                     arraysize(kT0IpAddresses),
                                     &expected_ip_addresses);
  ASSERT_TRUE(rv);

  int rv0 = t.Start();
  EXPECT_EQ(ERR_IO_PENDING, rv0);

  MessageLoop::current()->Run();

  EXPECT_TRUE(DnsTransaction::Key(t0_dns_name, kT1Qtype) == t.key());
  EXPECT_EQ(OK, delegate.result());
  EXPECT_EQ(&t, delegate.transaction());
  EXPECT_TRUE(expected_ip_addresses == delegate.ip_addresses());

  EXPECT_TRUE(socket0_data->at_read_eof());
  EXPECT_TRUE(socket0_data->at_write_eof());
  EXPECT_TRUE(socket1_data->at_read_eof());
  EXPECT_TRUE(socket1_data->at_write_eof());
  EXPECT_TRUE(socket2_data->at_read_eof());
  EXPECT_TRUE(socket2_data->at_write_eof());
  EXPECT_EQ(3u, factory.udp_client_sockets().size());
}

// Test that after the first timeout we do a fresh connection, and after
// the second timeout we do another fresh connection and after the third
// timeout we give up and return a timeout error.
TEST(DnsTransactionTest, ThirdTimeoutTest) {
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
  MockClientSocketFactory factory;
  factory.AddSocketDataProvider(socket0_data.get());
  factory.AddSocketDataProvider(socket1_data.get());
  factory.AddSocketDataProvider(socket2_data.get());

  TestPrng test_prng(std::deque<int>(3, 0));
  RandIntCallback rand_int_cb =
      base::Bind(&TestPrng::GetNext, base::Unretained(&test_prng));
  std::string t0_dns_name(kT0DnsName, arraysize(kT0DnsName));

  IPEndPoint dns_server;
  bool rv = CreateDnsAddress(kDnsIp, kDnsPort, &dns_server);
  ASSERT_TRUE(rv);

  DnsTransaction t(dns_server, t0_dns_name, kT1Qtype, rand_int_cb, &factory,
                   BoundNetLog(), NULL);

  TestDelegate delegate;
  t.SetDelegate(&delegate);

  t.set_timeouts_ms(
      std::vector<base::TimeDelta>(kTimeoutsMs,
                                   kTimeoutsMs + arraysize(kTimeoutsMs)));

  int rv0 = t.Start();
  EXPECT_EQ(ERR_IO_PENDING, rv0);

  MessageLoop::current()->Run();

  EXPECT_TRUE(DnsTransaction::Key(t0_dns_name, kT0Qtype) == t.key());
  EXPECT_EQ(ERR_DNS_TIMED_OUT, delegate.result());
  EXPECT_EQ(&t, delegate.transaction());

  EXPECT_TRUE(socket0_data->at_read_eof());
  EXPECT_TRUE(socket0_data->at_write_eof());
  EXPECT_TRUE(socket1_data->at_read_eof());
  EXPECT_TRUE(socket1_data->at_write_eof());
  EXPECT_TRUE(socket2_data->at_read_eof());
  EXPECT_TRUE(socket2_data->at_write_eof());
  EXPECT_EQ(3u, factory.udp_client_sockets().size());
}

}  // namespace net
