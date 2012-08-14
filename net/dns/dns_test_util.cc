// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_test_util.h"

#include <string>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/sys_byteorder.h"
#include "net/base/big_endian.h"
#include "net/base/dns_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_transaction.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

// A DnsTransaction which responds with loopback to all queries starting with
// "ok", fails synchronously on all queries starting with "er", and NXDOMAIN to
// all others.
class MockTransaction : public DnsTransaction,
                        public base::SupportsWeakPtr<MockTransaction> {
 public:
  MockTransaction(const std::string& hostname,
                  uint16 qtype,
                  const DnsTransactionFactory::CallbackType& callback)
      : hostname_(hostname),
        qtype_(qtype),
        callback_(callback),
        started_(false) {
  }

  virtual const std::string& GetHostname() const OVERRIDE {
    return hostname_;
  }

  virtual uint16 GetType() const OVERRIDE {
    return qtype_;
  }

  virtual int Start() OVERRIDE {
    EXPECT_FALSE(started_);
    started_ = true;
    if (hostname_.substr(0, 2) == "er")
      return ERR_NAME_NOT_RESOLVED;
    // Using WeakPtr to cleanly cancel when transaction is destroyed.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&MockTransaction::Finish, AsWeakPtr()));
    return ERR_IO_PENDING;
  }

 private:
  void Finish() {
    if (hostname_.substr(0, 2) == "ok") {
      std::string qname;
      DNSDomainFromDot(hostname_, &qname);
      DnsQuery query(0, qname, qtype_);

      DnsResponse response;
      char* buffer = response.io_buffer()->data();
      int nbytes = query.io_buffer()->size();
      memcpy(buffer, query.io_buffer()->data(), nbytes);

      const uint16 kPointerToQueryName =
          static_cast<uint16>(0xc000 | sizeof(net::dns_protocol::Header));

      const uint32 kTTL = 86400;  // One day.

      // Size of RDATA which is a IPv4 or IPv6 address.
      size_t rdata_size = qtype_ == net::dns_protocol::kTypeA ?
                          net::kIPv4AddressSize : net::kIPv6AddressSize;

      // 12 is the sum of sizes of the compressed name reference, TYPE,
      // CLASS, TTL and RDLENGTH.
      size_t answer_size = 12 + rdata_size;

      // Write answer with loopback IP address.
      reinterpret_cast<dns_protocol::Header*>(buffer)->ancount =
          base::HostToNet16(1);
      BigEndianWriter writer(buffer + nbytes, answer_size);
      writer.WriteU16(kPointerToQueryName);
      writer.WriteU16(qtype_);
      writer.WriteU16(net::dns_protocol::kClassIN);
      writer.WriteU32(kTTL);
      writer.WriteU16(rdata_size);
      if (qtype_ == net::dns_protocol::kTypeA) {
        char kIPv4Loopback[] = { 0x7f, 0, 0, 1 };
        writer.WriteBytes(kIPv4Loopback, sizeof(kIPv4Loopback));
      } else {
        char kIPv6Loopback[] = { 0, 0, 0, 0, 0, 0, 0, 0,
                                 0, 0, 0, 0, 0, 0, 0, 1 };
        writer.WriteBytes(kIPv6Loopback, sizeof(kIPv6Loopback));
      }

      EXPECT_TRUE(response.InitParse(nbytes + answer_size, query));
      callback_.Run(this, OK, &response);
    } else {
      callback_.Run(this, ERR_NAME_NOT_RESOLVED, NULL);
    }
  }

  const std::string hostname_;
  const uint16 qtype_;
  DnsTransactionFactory::CallbackType callback_;
  bool started_;
};


// A DnsTransactionFactory which creates MockTransaction.
class MockTransactionFactory : public DnsTransactionFactory {
 public:
  MockTransactionFactory() {}
  virtual ~MockTransactionFactory() {}

  virtual scoped_ptr<DnsTransaction> CreateTransaction(
      const std::string& hostname,
      uint16 qtype,
      const DnsTransactionFactory::CallbackType& callback,
      const BoundNetLog&) OVERRIDE {
    return scoped_ptr<DnsTransaction>(
        new MockTransaction(hostname, qtype, callback));
  }
};

// MockDnsClient provides MockTransactionFactory.
class MockDnsClient : public DnsClient {
 public:
  explicit MockDnsClient(const DnsConfig& config) : config_(config) {}
  virtual ~MockDnsClient() {}

  virtual void SetConfig(const DnsConfig& config) OVERRIDE {
    config_ = config;
  }

  virtual const DnsConfig* GetConfig() const OVERRIDE {
    return config_.IsValid() ? &config_ : NULL;
  }

  virtual DnsTransactionFactory* GetTransactionFactory() OVERRIDE {
    return config_.IsValid() ? &factory_ : NULL;
  }

 private:
  DnsConfig config_;
  MockTransactionFactory factory_;
};

}  // namespace

// static
scoped_ptr<DnsClient> CreateMockDnsClient(const DnsConfig& config) {
  return scoped_ptr<DnsClient>(new MockDnsClient(config));
}

MockDnsConfigService::~MockDnsConfigService() {
}

void MockDnsConfigService::OnDNSChanged(unsigned detail) {
}

void MockDnsConfigService::ChangeConfig(const DnsConfig& config) {
  DnsConfigService::OnConfigRead(config);
}

void MockDnsConfigService::ChangeHosts(const DnsHosts& hosts) {
  DnsConfigService::OnHostsRead(hosts);
}

}  // namespace net
