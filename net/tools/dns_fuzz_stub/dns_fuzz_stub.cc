// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "base/values.h"
#include "net/base/address_list.h"
#include "net/base/dns_util.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"

namespace {

bool FitsUint8(int num) {
  return (num >= 0) && (num <= kuint8max);
}

bool FitsUint16(int num) {
  return (num >= 0) && (num <= kuint16max);
}

bool ReadTestCase(const char* filename,
                  uint16* id, std::string* qname, uint16* qtype,
                  std::vector<char>* resp_buf) {
  FilePath filepath = FilePath::FromUTF8Unsafe(filename);

  std::string json;
  if (!file_util::ReadFileToString(filepath, &json)) {
    LOG(ERROR) << "Couldn't read file " << filename << ".";
    return false;
  }

  scoped_ptr<Value> value(base::JSONReader::Read(json));
  if (!value.get()) {
    LOG(ERROR) << "Couldn't parse JSON in " << filename << ".";
    return false;
  }

  DictionaryValue* dict;
  if (!value->GetAsDictionary(&dict)) {
    LOG(ERROR) << "Test case is not a dictionary.";
    return false;
  }

  int id_int;
  if (!dict->GetInteger("id", &id_int)) {
    LOG(ERROR) << "id is missing or not an integer.";
    return false;
  }
  if (!FitsUint16(id_int)) {
    LOG(ERROR) << "id is out of range.";
    return false;
  }
  *id = static_cast<uint16>(id_int);

  if (!dict->GetStringASCII("qname", qname)) {
    LOG(ERROR) << "qname is missing or not a string.";
    return false;
  }

  int qtype_int;
  if (!dict->GetInteger("qtype", &qtype_int)) {
    LOG(ERROR) << "qtype is missing or not an integer.";
    return false;
  }
  if (!FitsUint16(qtype_int)) {
    LOG(ERROR) << "qtype is out of range.";
    return false;
  }
  *qtype = static_cast<uint16>(qtype_int);

  ListValue* resp_list;
  if (!dict->GetList("response", &resp_list)) {
    LOG(ERROR) << "response is missing or not a list.";
    return false;
  }

  size_t resp_size = resp_list->GetSize();
  resp_buf->clear();
  resp_buf->reserve(resp_size);
  for (size_t i = 0; i < resp_size; i++) {
    int resp_byte_int;
    if ((!resp_list->GetInteger(i, &resp_byte_int))) {
      LOG(ERROR) << "response[" << i << "] is not an integer.";
      return false;
    }
    if (!FitsUint8(resp_byte_int)) {
      LOG(ERROR) << "response[" << i << "] is out of range.";
      return false;
    }
    resp_buf->push_back(static_cast<char>(resp_byte_int));
  }
  DCHECK(resp_buf->size() == resp_size);

  return true;
}

void RunTestCase(uint16 id, std::string& qname, uint16 qtype,
                 std::vector<char>& resp_buf) {
  net::DnsQuery query(id, qname, qtype);
  net::DnsResponse response;
  std::copy(resp_buf.begin(), resp_buf.end(), response.io_buffer()->data());

  if (!response.InitParse(resp_buf.size(), query)) {
    LOG(INFO) << "InitParse failed.";
    return;
  }

  net::AddressList address_list;
  base::TimeDelta ttl;
  net::DnsResponse::Result result = response.ParseToAddressList(
      &address_list, &ttl);
  if (result != net::DnsResponse::DNS_SUCCESS) {
    LOG(INFO) << "ParseToAddressList failed: " << result;
    return;
  }

  LOG(INFO) << "Address List:";
  for (unsigned int i = 0; i < address_list.size(); i++) {
    LOG(INFO) << "\t" << address_list[i].ToString();
  }
  LOG(INFO) << "TTL: " << ttl.InSeconds() << " seconds";
}

}

int main(int argc, char** argv) {
  if (argc != 2) {
    LOG(ERROR) << "Usage: " << argv[0] << " test_case_filename";
    return 1;
  }

  const char* filename = argv[1];

  LOG(INFO) << "Test case: " << filename;

  uint16 id = 0;
  std::string qname_dotted;
  uint16 qtype = 0;
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

  RunTestCase(id, qname, qtype, resp_buf);

  std::cout << "#EOF" << std::endl;

  return 0;
}

