// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/time.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/address_list.h"
#include "net/base/dns_util.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"

namespace {

// TODO(ttuttle): This should read from the file, probably in JSON.
bool ReadTestCase(const char* filename,
                  uint16* id, std::string* qname, uint16* qtype,
                  std::vector<char>* resp_buf) {
  static const unsigned char resp_bytes[] = {
    0x00, 0x00, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x61, 0x00, 0x00, 0x01, 0x00, 0x01,
    0x01, 0x61, 0x00, 0x00, 0x01, 0x00, 0x01,
    0x00, 0x00, 0x00, 0xff, 0x00, 0x04, 0x0a, 0x0a, 0x0a, 0x0a };

  *id = 0x0000;
  *qname = "a";
  *qtype = 0x0001;
  resp_buf->assign(resp_bytes, resp_bytes + arraysize(resp_bytes));

  return true;
}

}

int main(int argc, char** argv) {
  if (argc != 2) {
    LOG(ERROR) << "Usage: " << argv[0] << " test_case_filename";
    return 1;
  }

  const char* filename = argv[1];

  LOG(INFO) << "Test case: " << filename;

  uint16 id;
  std::string qname_dotted;
  uint16 qtype;
  std::vector<char> resp_buf;

  if (!ReadTestCase(filename, &id, &qname_dotted, &qtype, &resp_buf)) {
    LOG(ERROR) << "Test case format invalid";
    return 2;
  }

  LOG(INFO) << "Query: id=" << id
            << " qname=" << qname_dotted
            << " qtype=" << qtype;
  LOG(INFO) << "Response: " << resp_buf.size() << " bytes";

  std::string qname;
  if (!net::DNSDomainFromDot(qname_dotted, &qname)) {
    LOG(ERROR) << "DNSDomainFromDot(" << qname_dotted << ") failed.";
    return 3;
  }

  net::DnsQuery query(id, qname, qtype);
  net::DnsResponse response;
  std::copy(resp_buf.begin(), resp_buf.end(), response.io_buffer()->data());

  if (!response.InitParse(resp_buf.size(), query)) {
    LOG(INFO) << "InitParse failed.";
    return 0;
  }

  net::AddressList address_list;
  base::TimeDelta ttl;
  net::DnsResponse::Result result = response.ParseToAddressList(
      &address_list, &ttl);
  if (result != net::DnsResponse::DNS_SUCCESS) {
    LOG(INFO) << "ParseToAddressList failed: " << result;
    return 0;
  }

  LOG(INFO) << "Address List:";
  for (unsigned int i = 0; i < address_list.size(); i++) {
    LOG(INFO) << "\t" << address_list[i].ToString();
  }
  LOG(INFO) << "TTL: " << ttl.InSeconds() << " seconds";

  return 0;
}

