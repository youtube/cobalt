// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/pem.h"

#include "base/base64.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace {

const char kPEMSearchBlock[] = "-----BEGIN ";
const char kPEMBeginBlock[] = "-----BEGIN %s-----";
const char kPEMEndBlock[] = "-----END %s-----";

}  // namespace

namespace net {

using base::StringPiece;

struct PEMTokenizer::PEMType {
  std::string type;
  std::string header;
  std::string footer;
};

PEMTokenizer::PEMTokenizer(
    StringPiece str,
    const std::vector<std::string>& allowed_block_types) {
  Init(str, allowed_block_types);
}

PEMTokenizer::~PEMTokenizer() = default;

bool PEMTokenizer::GetNext() {
  while (pos_ != StringPiece::npos) {
    // Scan for the beginning of the next PEM encoded block.
    pos_ = str_.find(kPEMSearchBlock, pos_);
    if (pos_ == StringPiece::npos)
      return false;  // No more PEM blocks

    std::vector<PEMType>::const_iterator it;
    // Check to see if it is of an acceptable block type.
    for (it = block_types_.begin(); it != block_types_.end(); ++it) {
      if (!base::StartsWith(str_.substr(pos_), it->header))
        continue;

      // Look for a footer matching the header. If none is found, then all
      // data following this point is invalid and should not be parsed.
      StringPiece::size_type footer_pos = str_.find(it->footer, pos_);
      if (footer_pos == StringPiece::npos) {
        pos_ = StringPiece::npos;
        return false;
      }

      // Chop off the header and footer and parse the data in between.
      StringPiece::size_type data_begin = pos_ + it->header.size();
      pos_ = footer_pos + it->footer.size();
      block_type_ = it->type;

      StringPiece encoded = str_.substr(data_begin, footer_pos - data_begin);
      if (!base::Base64Decode(base::CollapseWhitespaceASCII(encoded, true),
                              &data_)) {
        // The most likely cause for a decode failure is a datatype that
        // includes PEM headers, which are not supported.
        break;
      }

      return true;
    }

    // If the block did not match any acceptable type, move past it and
    // continue the search. Otherwise, |pos_| has been updated to the most
    // appropriate search position to continue searching from and should not
    // be adjusted.
    if (it == block_types_.end())
      pos_ += sizeof(kPEMSearchBlock);
  }

  return false;
}

void PEMTokenizer::Init(StringPiece str,
                        const std::vector<std::string>& allowed_block_types) {
  str_ = str;
  pos_ = 0;

  // Construct PEM header/footer strings for all the accepted types, to
  // reduce parsing later.
  for (const auto& allowed_block_type : allowed_block_types) {
    PEMType allowed_type;
    allowed_type.type = allowed_block_type;
    allowed_type.header =
        base::StringPrintf(kPEMBeginBlock, allowed_block_type.c_str());
    allowed_type.footer =
        base::StringPrintf(kPEMEndBlock, allowed_block_type.c_str());
    block_types_.push_back(allowed_type);
  }
}

std::string PEMEncode(base::StringPiece data, const std::string& type) {
  std::string b64_encoded;
  base::Base64Encode(data, &b64_encoded);

  // Divide the Base-64 encoded data into 64-character chunks, as per
  // 4.3.2.4 of RFC 1421.
  static const size_t kChunkSize = 64;
  size_t chunks = (b64_encoded.size() + (kChunkSize - 1)) / kChunkSize;

  std::string pem_encoded;
  pem_encoded.reserve(
      // header & footer
      17 + 15 + type.size() * 2 +
      // encoded data
      b64_encoded.size() +
      // newline characters for line wrapping in encoded data
      chunks);

  pem_encoded = "-----BEGIN ";
  pem_encoded.append(type);
  pem_encoded.append("-----\n");

  for (size_t i = 0, chunk_offset = 0; i < chunks;
       ++i, chunk_offset += kChunkSize) {
    pem_encoded.append(b64_encoded, chunk_offset, kChunkSize);
    pem_encoded.append("\n");
  }

  pem_encoded.append("-----END ");
  pem_encoded.append(type);
  pem_encoded.append("-----\n");
  return pem_encoded;
}

}  // namespace net
