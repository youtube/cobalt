// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/opt_record_rdata.h"

#include <algorithm>
#include <memory>
#include <numeric>
#include <utility>

#include "base/big_endian.h"
#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "net/dns/public/dns_protocol.h"

namespace net {

namespace {
std::string SerializeEdeOpt(uint16_t info_code, base::StringPiece extra_text) {
  std::string buf(2 + extra_text.size(), '\0');

  base::BigEndianWriter writer(buf.data(), buf.size());
  CHECK(writer.WriteU16(info_code));
  CHECK(writer.WriteBytes(extra_text.data(), extra_text.size()));
  return buf;
}
}  // namespace

OptRecordRdata::Opt::Opt(std::string data) : data_(std::move(data)) {}

bool OptRecordRdata::Opt::operator==(const OptRecordRdata::Opt& other) const {
  return IsEqual(other);
}

bool OptRecordRdata::Opt::operator!=(const OptRecordRdata::Opt& other) const {
  return !IsEqual(other);
}

bool OptRecordRdata::Opt::IsEqual(const OptRecordRdata::Opt& other) const {
  return GetCode() == other.GetCode() && data() == other.data();
}

OptRecordRdata::EdeOpt::EdeOpt(uint16_t info_code, std::string extra_text)
    : Opt(SerializeEdeOpt(info_code, extra_text)),
      info_code_(info_code),
      extra_text_(std::move(extra_text)) {
  CHECK(base::IsStringUTF8(extra_text_));
}

OptRecordRdata::EdeOpt::~EdeOpt() = default;

std::unique_ptr<OptRecordRdata::EdeOpt> OptRecordRdata::EdeOpt::Create(
    std::string data) {
  uint16_t info_code;
  base::StringPiece extra_text;
  auto edeReader = base::BigEndianReader::FromStringPiece(data);

  // size must be at least 2: info_code + optional extra_text
  if (!(edeReader.ReadU16(&info_code) &&
        edeReader.ReadPiece(&extra_text, edeReader.remaining()))) {
    return nullptr;
  }

  if (!base::IsStringUTF8(extra_text)) {
    return nullptr;
  }

  std::string extra_text_str(extra_text.data(), extra_text.size());
  return std::make_unique<EdeOpt>(info_code, std::move(extra_text_str));
}

uint16_t OptRecordRdata::EdeOpt::GetCode() const {
  return EdeOpt::kOptCode;
}

OptRecordRdata::EdeOpt::EdeInfoCode
OptRecordRdata::EdeOpt::GetEnumFromInfoCode() const {
  return GetEnumFromInfoCode(info_code_);
}

OptRecordRdata::EdeOpt::EdeInfoCode OptRecordRdata::EdeOpt::GetEnumFromInfoCode(
    uint16_t info_code) {
  switch (info_code) {
    case 0:
      return EdeInfoCode::kOtherError;
    case 1:
      return EdeInfoCode::kUnsupportedDnskeyAlgorithm;
    case 2:
      return EdeInfoCode::kUnsupportedDsDigestType;
    case 3:
      return EdeInfoCode::kStaleAnswer;
    case 4:
      return EdeInfoCode::kForgedAnswer;
    case 5:
      return EdeInfoCode::kDnssecIndeterminate;
    case 6:
      return EdeInfoCode::kDnssecBogus;
    case 7:
      return EdeInfoCode::kSignatureExpired;
    case 8:
      return EdeInfoCode::kSignatureNotYetValid;
    case 9:
      return EdeInfoCode::kDnskeyMissing;
    case 10:
      return EdeInfoCode::kRrsigsMissing;
    case 11:
      return EdeInfoCode::kNoZoneKeyBitSet;
    case 12:
      return EdeInfoCode::kNsecMissing;
    case 13:
      return EdeInfoCode::kCachedError;
    case 14:
      return EdeInfoCode::kNotReady;
    case 15:
      return EdeInfoCode::kBlocked;
    case 16:
      return EdeInfoCode::kCensored;
    case 17:
      return EdeInfoCode::kFiltered;
    case 18:
      return EdeInfoCode::kProhibited;
    case 19:
      return EdeInfoCode::kStaleNxdomainAnswer;
    case 20:
      return EdeInfoCode::kNotAuthoritative;
    case 21:
      return EdeInfoCode::kNotSupported;
    case 22:
      return EdeInfoCode::kNoReachableAuthority;
    case 23:
      return EdeInfoCode::kNetworkError;
    case 24:
      return EdeInfoCode::kInvalidData;
    case 25:
      return EdeInfoCode::kSignatureExpiredBeforeValid;
    case 26:
      return EdeInfoCode::kTooEarly;
    case 27:
      return EdeInfoCode::kUnsupportedNsec3IterationsValue;
    default:
      return EdeInfoCode::kUnrecognizedErrorCode;
  }
}

OptRecordRdata::PaddingOpt::PaddingOpt(std::string padding)
    : Opt(std::move(padding)) {}

OptRecordRdata::PaddingOpt::PaddingOpt(uint16_t padding_len)
    : Opt(std::string(base::checked_cast<size_t>(padding_len), '\0')) {}

OptRecordRdata::PaddingOpt::~PaddingOpt() = default;

uint16_t OptRecordRdata::PaddingOpt::GetCode() const {
  return PaddingOpt::kOptCode;
}

OptRecordRdata::UnknownOpt::~UnknownOpt() = default;

std::unique_ptr<OptRecordRdata::UnknownOpt>
OptRecordRdata::UnknownOpt::CreateForTesting(uint16_t code, std::string data) {
  CHECK_IS_TEST();
  return base::WrapUnique(
      new OptRecordRdata::UnknownOpt(code, std::move(data)));
}

OptRecordRdata::UnknownOpt::UnknownOpt(uint16_t code, std::string data)
    : Opt(std::move(data)), code_(code) {
  CHECK(!base::Contains(kOptsWithDedicatedClasses, code));
}

uint16_t OptRecordRdata::UnknownOpt::GetCode() const {
  return code_;
}

OptRecordRdata::OptRecordRdata() = default;

OptRecordRdata::~OptRecordRdata() = default;

bool OptRecordRdata::operator==(const OptRecordRdata& other) const {
  return IsEqual(&other);
}

bool OptRecordRdata::operator!=(const OptRecordRdata& other) const {
  return !IsEqual(&other);
}

// static
std::unique_ptr<OptRecordRdata> OptRecordRdata::Create(base::StringPiece data) {
  auto rdata = std::make_unique<OptRecordRdata>();
  rdata->buf_.assign(data.begin(), data.end());

  auto reader = base::BigEndianReader::FromStringPiece(data);
  while (reader.remaining() > 0) {
    uint16_t opt_code, opt_data_size;
    base::StringPiece opt_data_sp;

    if (!(reader.ReadU16(&opt_code) && reader.ReadU16(&opt_data_size) &&
          reader.ReadPiece(&opt_data_sp, opt_data_size))) {
      return nullptr;
    }

    std::string opt_data{opt_data_sp.data(), opt_data_sp.size()};

    // After the Opt object has been parsed, parse the contents (the data)
    // depending on the opt_code. The specific Opt subclasses all inherit from
    // Opt. If an opt code does not have a matching Opt subclass, a simple Opt
    // object will be created, and data won't be parsed.

    std::unique_ptr<Opt> opt;

    switch (opt_code) {
      case dns_protocol::kEdnsPadding:
        opt = std::make_unique<OptRecordRdata::PaddingOpt>(std::move(opt_data));
        break;
      case dns_protocol::kEdnsExtendedDnsError:
        opt = OptRecordRdata::EdeOpt::Create(std::move(opt_data));
        break;
      default:
        opt = base::WrapUnique(
            new OptRecordRdata::UnknownOpt(opt_code, std::move(opt_data)));
        break;
    }

    // Confirm that opt is not null, which would be the result of a failed
    // parse.
    if (!opt) {
      return nullptr;
    }

    rdata->opts_.emplace(opt_code, std::move(opt));
  }

  return rdata;
}

uint16_t OptRecordRdata::Type() const {
  return OptRecordRdata::kType;
}

bool OptRecordRdata::IsEqual(const RecordRdata* other) const {
  if (other->Type() != Type()) {
    return false;
  }
  const OptRecordRdata* opt_other = static_cast<const OptRecordRdata*>(other);
  return opt_other->buf_ == buf_;
}

void OptRecordRdata::AddOpt(std::unique_ptr<Opt> opt) {
  base::StringPiece opt_data = opt->data();

  // Resize buffer to accommodate new OPT.
  const size_t orig_rdata_size = buf_.size();
  buf_.resize(orig_rdata_size + Opt::kHeaderSize + opt_data.size());

  // Start writing from the end of the existing rdata.
  base::BigEndianWriter writer(buf_.data(), buf_.size());
  CHECK(writer.Skip(orig_rdata_size));
  bool success = writer.WriteU16(opt->GetCode()) &&
                 writer.WriteU16(opt_data.size()) &&
                 writer.WriteBytes(opt_data.data(), opt_data.size());
  DCHECK(success);

  opts_.emplace(opt->GetCode(), std::move(opt));
}

bool OptRecordRdata::ContainsOptCode(uint16_t opt_code) const {
  return opts_.find(opt_code) != opts_.end();
}

std::vector<const OptRecordRdata::Opt*> OptRecordRdata::GetOpts() const {
  std::vector<const OptRecordRdata::Opt*> opts;
  opts.reserve(OptCount());
  for (const auto& elem : opts_) {
    opts.push_back(elem.second.get());
  }
  return opts;
}

std::vector<const OptRecordRdata::PaddingOpt*> OptRecordRdata::GetPaddingOpts()
    const {
  std::vector<const OptRecordRdata::PaddingOpt*> opts;
  auto range = opts_.equal_range(dns_protocol::kEdnsPadding);
  for (auto it = range.first; it != range.second; ++it) {
    opts.push_back(static_cast<const PaddingOpt*>(it->second.get()));
  }
  return opts;
}

std::vector<const OptRecordRdata::EdeOpt*> OptRecordRdata::GetEdeOpts() const {
  std::vector<const OptRecordRdata::EdeOpt*> opts;
  auto range = opts_.equal_range(dns_protocol::kEdnsExtendedDnsError);
  for (auto it = range.first; it != range.second; ++it) {
    opts.push_back(static_cast<const EdeOpt*>(it->second.get()));
  }
  return opts;
}

}  // namespace net
