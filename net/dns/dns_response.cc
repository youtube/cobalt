// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_response.h"

#include "net/base/big_endian.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/sys_byteorder.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/dns_query.h"

namespace net {

DnsResourceRecord::DnsResourceRecord() {
}

DnsResourceRecord::~DnsResourceRecord() {
}

DnsRecordParser::DnsRecordParser() : packet_(NULL), length_(0), cur_(0) {
}

DnsRecordParser::DnsRecordParser(const void* packet,
                                 size_t length,
                                 size_t offset)
    : packet_(reinterpret_cast<const char*>(packet)),
      length_(length),
      cur_(packet_ + offset) {
  DCHECK_LE(offset, length);
}

int DnsRecordParser::ParseName(const void* const vpos, std::string* out) const {
  const char* const pos = reinterpret_cast<const char*>(vpos);
  DCHECK(packet_);
  DCHECK_LE(packet_, pos);
  DCHECK_LE(pos, packet_ + length_);

  const char* p = pos;
  const char* end = packet_ + length_;
  // Count number of seen bytes to detect loops.
  size_t seen = 0;
  // Remember how many bytes were consumed before first jump.
  size_t consumed = 0;

  if (pos >= end)
    return 0;

  if (out) {
    out->clear();
    out->reserve(dns_protocol::kMaxNameLength);
  }

  for (;;) {
    // The two couple of bits of the length give the type of the length. It's
    // either a direct length or a pointer to the remainder of the name.
    switch (*p & dns_protocol::kLabelMask) {
      case dns_protocol::kLabelPointer: {
        if (p + sizeof(uint16) > end)
          return 0;
        if (consumed == 0) {
          consumed = p - pos + sizeof(uint16);
          if (!out)
            return consumed;  // If name is not stored, that's all we need.
        }
        seen += sizeof(uint16);
        // If seen the whole packet, then we must be in a loop.
        if (seen > length_)
          return 0;
        uint16 offset;
        ReadBigEndian<uint16>(p, &offset);
        offset &= dns_protocol::kOffsetMask;
        p = packet_ + offset;
        if (p >= end)
          return 0;
        break;
      }
      case dns_protocol::kLabelDirect: {
        uint8 label_len = *p;
        ++p;
        // Note: root domain (".") is NOT included.
        if (label_len == 0) {
          if (consumed == 0) {
            consumed = p - pos;
          }  // else we set |consumed| before first jump
          return consumed;
        }
        if (p + label_len >= end)
          return 0;  // Truncated or missing label.
        if (out) {
          if (!out->empty())
            out->append(".");
          out->append(p, label_len);
        }
        p += label_len;
        seen += 1 + label_len;
        break;
      }
      default:
        // unhandled label type
        return 0;
    }
  }
}

bool DnsRecordParser::ParseRecord(DnsResourceRecord* out) {
  DCHECK(packet_);
  size_t consumed = ParseName(cur_, &out->name);
  if (!consumed)
    return false;
  BigEndianReader reader(cur_ + consumed,
                         packet_ + length_ - (cur_ + consumed));
  uint16 rdlen;
  if (reader.ReadU16(&out->type) &&
      reader.ReadU16(&out->klass) &&
      reader.ReadU32(&out->ttl) &&
      reader.ReadU16(&rdlen) &&
      reader.ReadPiece(&out->rdata, rdlen)) {
    cur_ = reader.ptr();
    return true;
  }
  return false;
}

DnsResponse::DnsResponse()
    : io_buffer_(new IOBufferWithSize(dns_protocol::kMaxUDPSize + 1)) {
}

DnsResponse::DnsResponse(const void* data,
                         size_t length,
                         size_t answer_offset)
    : io_buffer_(new IOBufferWithSize(length)),
      parser_(io_buffer_->data(), length, answer_offset) {
  memcpy(io_buffer_->data(), data, length);
}

DnsResponse::~DnsResponse() {
}

bool DnsResponse::InitParse(int nbytes, const DnsQuery& query) {
  // Response includes query, it should be at least that size.
  if (nbytes < query.io_buffer()->size() || nbytes > dns_protocol::kMaxUDPSize)
    return false;

  // Match the query id.
  if (ntohs(header()->id) != query.id())
    return false;

  // Match question count.
  if (ntohs(header()->qdcount) != 1)
    return false;

  // Match the question section.
  const size_t hdr_size = sizeof(dns_protocol::Header);
  const base::StringPiece question = query.question();
  if (question != base::StringPiece(io_buffer_->data() + hdr_size,
                                    question.size())) {
    return false;
  }

  // Construct the parser.
  parser_ = DnsRecordParser(io_buffer_->data(),
                            nbytes,
                            hdr_size + question.size());
  return true;
}

uint8 DnsResponse::flags0() const {
  return header()->flags[0];
}

uint8 DnsResponse::flags1() const {
  return header()->flags[1] & ~(dns_protocol::kRcodeMask);
}

uint8 DnsResponse::rcode() const {
  return header()->flags[1] & dns_protocol::kRcodeMask;
}

int DnsResponse::answer_count() const {
  return ntohs(header()->ancount);
}

DnsRecordParser DnsResponse::Parser() const {
  DCHECK(parser_.IsValid());
  return parser_;
}

const dns_protocol::Header* DnsResponse::header() const {
  return reinterpret_cast<const dns_protocol::Header*>(io_buffer_->data());
}

}  // namespace net
