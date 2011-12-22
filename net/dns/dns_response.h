// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_RESPONSE_H_
#define NET_DNS_DNS_RESPONSE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/string_piece.h"
#include "net/base/net_export.h"
#include "net/base/net_util.h"

namespace net {

class DnsQuery;
class IOBufferWithSize;

namespace dns_protocol {
struct Header;
}

// Parsed resource record.
struct NET_EXPORT_PRIVATE DnsResourceRecord {
  DnsResourceRecord();
  ~DnsResourceRecord();

  std::string name;  // in dotted form
  uint16 type;
  uint16 klass;
  uint32 ttl;
  base::StringPiece rdata;  // points to the original response buffer
};

// Iterator to walk over resource records of the DNS response packet.
class NET_EXPORT_PRIVATE DnsRecordParser {
 public:
  // Construct an uninitialized iterator.
  DnsRecordParser();

  // Construct an iterator to process the |packet| of given |length|.
  // |offset| points to the beginning of the answer section.
  DnsRecordParser(const void* packet, size_t length, size_t offset);

  // Returns |true| if initialized.
  bool IsValid() const { return packet_ != NULL; }

  // Returns |true| if no more bytes remain in the packet.
  bool AtEnd() const { return cur_ == packet_ + length_; }

  // Parses a (possibly compressed) DNS name from the packet starting at
  // |pos|. Stores output (even partial) in |out| unless |out| is NULL. |out|
  // is stored in the dotted form, e.g., "example.com". Returns number of bytes
  // consumed or 0 on failure.
  // This is exposed to allow parsing compressed names within RRDATA for TYPEs
  // such as NS, CNAME, PTR, MX, SOA.
  // See RFC 1035 section 4.1.4.
  int ParseName(const void* pos, std::string* out) const;

  // Parses the next resource record. Returns true if succeeded.
  bool ParseRecord(DnsResourceRecord* record);

 private:
  const char* packet_;
  size_t length_;
  // Current offset within the packet.
  const char* cur_;
};

// Buffer-holder for the DNS response allowing easy access to the header fields
// and resource records. After reading into |io_buffer| must call InitParse to
// position the RR parser.
class NET_EXPORT_PRIVATE DnsResponse {
 public:
  // Constructs an object with an IOBuffer large enough to read
  // one byte more than largest possible response, to detect malformed
  // responses.
  DnsResponse();
  // Constructs response from |data|. Used for testing purposes only!
  DnsResponse(const void* data, size_t length, size_t answer_offset);
  ~DnsResponse();

  // Internal buffer accessor into which actual bytes of response will be
  // read.
  IOBufferWithSize* io_buffer() { return io_buffer_.get(); }

  // Returns false if the packet is shorter than the header or does not match
  // |query| id or question.
  bool InitParse(int nbytes, const DnsQuery& query);

  // Accessors for the header.
  uint8 flags0() const;  // first byte of flags
  uint8 flags1() const;  // second byte of flags excluding rcode
  uint8 rcode() const;
  int answer_count() const;

  // Returns an iterator to the resource records in the answer section. Must be
  // called after InitParse. The iterator is valid only in the scope of the
  // DnsResponse.
  DnsRecordParser Parser() const;

 private:
  // Convenience for header access.
  const dns_protocol::Header* header() const;

  // Buffer into which response bytes are read.
  scoped_refptr<IOBufferWithSize> io_buffer_;

  // Iterator constructed after InitParse positioned at the answer section.
  DnsRecordParser parser_;

  DISALLOW_COPY_AND_ASSIGN(DnsResponse);
};

}  // namespace net

#endif  // NET_DNS_DNS_RESPONSE_H_
