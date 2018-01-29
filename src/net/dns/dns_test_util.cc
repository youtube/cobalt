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
#include "net/dns/address_sorter.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_transaction.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

// A DnsTransaction which uses MockDnsClientRuleList to determine the response.
class MockTransaction : public DnsTransaction,
                        public base::SupportsWeakPtr<MockTransaction> {
 public:
  MockTransaction(const MockDnsClientRuleList& rules,
                  const std::string& hostname,
                  uint16 qtype,
                  const DnsTransactionFactory::CallbackType& callback)
      : result_(MockDnsClientRule::FAIL_SYNC),
        hostname_(hostname),
        qtype_(qtype),
        callback_(callback),
        started_(false) {
    // Find the relevant rule which matches |qtype| and prefix of |hostname|.
    for (size_t i = 0; i < rules.size(); ++i) {
      const std::string& prefix = rules[i].prefix;
      if ((rules[i].qtype == qtype) &&
          (hostname.size() >= prefix.size()) &&
          (hostname.compare(0, prefix.size(), prefix) == 0)) {
        result_ = rules[i].result;
        break;
      }
    }
  }

  virtual const std::string& GetHostname() const override {
    return hostname_;
  }

  virtual uint16 GetType() const override {
    return qtype_;
  }

  virtual int Start() override {
    EXPECT_FALSE(started_);
    started_ = true;
    if (MockDnsClientRule::FAIL_SYNC == result_)
      return ERR_NAME_NOT_RESOLVED;
    // Using WeakPtr to cleanly cancel when transaction is destroyed.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&MockTransaction::Finish, AsWeakPtr()));
    return ERR_IO_PENDING;
  }

 private:
  void Finish() {
    switch (result_) {
      case MockDnsClientRule::EMPTY:
      case MockDnsClientRule::OK: {
        std::string qname;
        DNSDomainFromDot(hostname_, &qname);
        DnsQuery query(0, qname, qtype_);

        DnsResponse response;
        char* buffer = response.io_buffer()->data();
        int nbytes = query.io_buffer()->size();
        memcpy(buffer, query.io_buffer()->data(), nbytes);
        dns_protocol::Header* header =
            reinterpret_cast<dns_protocol::Header*>(buffer);
        header->flags |= dns_protocol::kFlagResponse;

        if (MockDnsClientRule::OK == result_) {
          const uint16 kPointerToQueryName =
              static_cast<uint16>(0xc000 | sizeof(*header));

          const uint32 kTTL = 86400;  // One day.

          // Size of RDATA which is a IPv4 or IPv6 address.
          size_t rdata_size = qtype_ == net::dns_protocol::kTypeA ?
                              net::kIPv4AddressSize : net::kIPv6AddressSize;

          // 12 is the sum of sizes of the compressed name reference, TYPE,
          // CLASS, TTL and RDLENGTH.
          size_t answer_size = 12 + rdata_size;

          // Write answer with loopback IP address.
          header->ancount = base::HostToNet16(1);
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
          nbytes += answer_size;
        }
        EXPECT_TRUE(response.InitParse(nbytes, query));
        callback_.Run(this, OK, &response);
      } break;
      case MockDnsClientRule::FAIL_ASYNC:
        callback_.Run(this, ERR_NAME_NOT_RESOLVED, NULL);
        break;
      case MockDnsClientRule::TIMEOUT:
        callback_.Run(this, ERR_DNS_TIMED_OUT, NULL);
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  MockDnsClientRule::Result result_;
  const std::string hostname_;
  const uint16 qtype_;
  DnsTransactionFactory::CallbackType callback_;
  bool started_;
};


// A DnsTransactionFactory which creates MockTransaction.
class MockTransactionFactory : public DnsTransactionFactory {
 public:
  explicit MockTransactionFactory(const MockDnsClientRuleList& rules)
      : rules_(rules) {}
  virtual ~MockTransactionFactory() {}

  virtual scoped_ptr<DnsTransaction> CreateTransaction(
      const std::string& hostname,
      uint16 qtype,
      const DnsTransactionFactory::CallbackType& callback,
      const BoundNetLog&) override {
    return scoped_ptr<DnsTransaction>(
        new MockTransaction(rules_, hostname, qtype, callback));
  }

 private:
  MockDnsClientRuleList rules_;
};

class MockAddressSorter : public AddressSorter {
 public:
  virtual ~MockAddressSorter() {}
  virtual void Sort(const AddressList& list,
                    const CallbackType& callback) const override {
    // Do nothing.
    callback.Run(true, list);
  }
};

// MockDnsClient provides MockTransactionFactory.
class MockDnsClient : public DnsClient {
 public:
  MockDnsClient(const DnsConfig& config,
                const MockDnsClientRuleList& rules)
      : config_(config), factory_(rules) {}
  virtual ~MockDnsClient() {}

  virtual void SetConfig(const DnsConfig& config) override {
    config_ = config;
  }

  virtual const DnsConfig* GetConfig() const override {
    return config_.IsValid() ? &config_ : NULL;
  }

  virtual DnsTransactionFactory* GetTransactionFactory() override {
    return config_.IsValid() ? &factory_ : NULL;
  }

  virtual AddressSorter* GetAddressSorter() override {
    return &address_sorter_;
  }

 private:
  DnsConfig config_;
  MockTransactionFactory factory_;
  MockAddressSorter address_sorter_;
};

}  // namespace

// static
scoped_ptr<DnsClient> CreateMockDnsClient(const DnsConfig& config,
                                          const MockDnsClientRuleList& rules) {
  return scoped_ptr<DnsClient>(new MockDnsClient(config, rules));
}

}  // namespace net
