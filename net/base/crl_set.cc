// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "crypto/sha2.h"
#include "net/base/crl_set.h"

#if defined(USE_SYSTEM_ZLIB)
#include <zlib.h>
#else
#include "third_party/zlib/zlib.h"
#endif

namespace net {

// Decompress zlib decompressed |in| into |out|. |out_len| is the number of
// bytes at |out| and must be exactly equal to the size of the decompressed
// data.
static bool DecompressZlib(uint8* out, int out_len, base::StringPiece in) {
  z_stream z;
  memset(&z, 0, sizeof(z));

  z.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.data()));
  z.avail_in = in.size();
  z.next_out = reinterpret_cast<Bytef*>(out);
  z.avail_out = out_len;

  if (inflateInit(&z) != Z_OK)
    return false;
  bool ret = false;
  int r = inflate(&z, Z_FINISH);
  if (r != Z_STREAM_END)
    goto err;
  if (z.avail_in || z.avail_out)
    goto err;
  ret = true;

 err:
  inflateEnd(&z);
  return ret;
}

CRLSet::CRLSet()
    : sequence_(0) {
}

CRLSet::~CRLSet() {
}

// CRLSet format:
//
// uint16le header_len
// byte[header_len] header_bytes
// repeated {
//   byte[32] parent_spki_sha256
//   uint32le num_serials
//   [num_serials] {
//     uint8 serial_length;
//     byte[serial_length] serial;
//   }
//
// header_bytes consists of a JSON dictionary with the following keys:
//   Version (int): currently 0
//   ContentType (string): "CRLSet" or "CRLSetDelta" (magic value)
//   DeltaFrom (int): if this is a delta update (see below), then this contains
//       the sequence number of the base CRLSet.
//   NextUpdate (int64, epoch seconds): the time at which an update is
//       availible.
//   WindowSecs (int64, seconds): the span of time to spread fetches over.
//   Sequence (int32): the monotonic sequence number of this CRL set.
//
// A delta CRLSet is similar to a CRLSet:
//
// struct CompressedChanges {
//    uint32le uncompressed_size
//    uint32le compressed_size
//    byte[compressed_size] zlib_data
// }
//
// uint16le header_len
// byte[header_len] header_bytes
// CompressedChanges crl_changes
// [crl_changes.uncompressed_size] {
//   switch (crl_changes[i]) {
//   case 0:
//     // CRL is the same
//   case 1:
//     // New CRL inserted
//     // See CRL structure from the non-delta format
//   case 2:
//     // CRL deleted
//   case 3:
//     // CRL changed
//     CompressedChanges serials_changes
//     [serials_changes.uncompressed_size] {
//       switch (serials_changes[i]) {
//       case 0:
//         // the serial is the same
//       case 1:
//         // serial inserted
//         uint8 serial_length
//         byte[serial_length] serial
//       case 2:
//         // serial deleted
//       }
//     }
//   }
// }
//
// A delta CRLSet applies to a specific CRL set as given in the
// header's "DeltaFrom" value. The delta describes the changes to each CRL
// in turn with a zlib compressed array of options: either the CRL is the case,
// a new CRL is inserted, the CRL is deleted or the CRL is updated. In the same
// of an update, the serials in the CRL are considered in the same fashion
// except there is no delta update of a serial number: they are either
// inserted, deleted or left the same.

// ReadHeader reads the header (including length prefix) from |data| and
// updates |data| to remove the header on return. Caller takes ownership of the
// returned pointer.
static DictionaryValue* ReadHeader(base::StringPiece* data) {
  if (data->size() < 2)
    return NULL;
  uint16 header_len;
  memcpy(&header_len, data->data(), 2);  // assumes little-endian.
  data->remove_prefix(2);

  if (data->size() < header_len)
    return NULL;

  const base::StringPiece header_bytes(data->data(), header_len);
  data->remove_prefix(header_len);

  scoped_ptr<Value> header(base::JSONReader::Read(
      header_bytes.as_string(), true /* allow trailing comma */));
  if (header.get() == NULL)
    return NULL;

  if (!header->IsType(Value::TYPE_DICTIONARY))
    return NULL;
  return reinterpret_cast<DictionaryValue*>(header.release());
}

// kCurrentFileVersion is the version of the CRLSet file format that we
// currently implement.
static const int kCurrentFileVersion = 0;

static bool ReadCRL(base::StringPiece* data, std::string* out_parent_spki_hash,
                    std::vector<std::string>* out_serials) {
  if (data->size() < crypto::kSHA256Length)
    return false;
  *out_parent_spki_hash = std::string(data->data(), crypto::kSHA256Length);
  data->remove_prefix(crypto::kSHA256Length);

  if (data->size() < sizeof(uint32))
    return false;
  uint32 num_serials;
  memcpy(&num_serials, data->data(), sizeof(uint32));  // assumes little endian
  data->remove_prefix(sizeof(uint32));

  for (uint32 i = 0; i < num_serials; i++) {
    uint8 serial_length;
    if (data->size() < sizeof(uint8))
      return false;
    memcpy(&serial_length, data->data(), sizeof(uint8));
    data->remove_prefix(sizeof(uint8));

    if (data->size() < serial_length)
      return false;
    std::string serial(data->data(), serial_length);
    data->remove_prefix(serial_length);
    out_serials->push_back(serial);
  }

  return true;
}

// static
bool CRLSet::Parse(base::StringPiece data, scoped_refptr<CRLSet>* out_crl_set) {
  // Other parts of Chrome assume that we're little endian, so we don't lose
  // anything by doing this.
#if defined(__BYTE_ORDER)
  // Linux check
  COMPILE_ASSERT(__BYTE_ORDER == __LITTLE_ENDIAN, assumes_little_endian);
#elif defined(__BIG_ENDIAN__)
  // Mac check
  #error assumes little endian
#endif

  scoped_ptr<DictionaryValue> header_dict(ReadHeader(&data));
  if (!header_dict.get())
    return false;

  std::string contents;
  if (!header_dict->GetString("ContentType", &contents))
    return false;
  if (contents != "CRLSet")
    return false;

  int version;
  if (!header_dict->GetInteger("Version", &version) ||
      version != kCurrentFileVersion) {
    return false;
  }

  double next_update, window_secs;
  int sequence;
  if (!header_dict->GetDouble("NextUpdate", &next_update) ||
      !header_dict->GetDouble("WindowSecs", &window_secs) ||
      !header_dict->GetInteger("Sequence", &sequence)) {
    return false;
  }

  scoped_refptr<CRLSet> crl_set(new CRLSet);
  crl_set->next_update_ = base::Time::FromDoubleT(next_update);
  crl_set->update_window_ =
      base::TimeDelta::FromSeconds(static_cast<int64>(window_secs));
  crl_set->sequence_ = static_cast<uint32>(sequence);

  for (size_t crl_index = 0; !data.empty(); crl_index++) {
    std::string parent_spki_sha256;
    std::vector<std::string> serials;
    if (!ReadCRL(&data, &parent_spki_sha256, &serials))
      return false;

    crl_set->crls_.push_back(std::make_pair(parent_spki_sha256, serials));
    crl_set->crls_index_by_issuer_[parent_spki_sha256] = crl_index;
  }

  *out_crl_set = crl_set;
  return true;
}

// kMaxUncompressedChangesLength is the largest changes array that we'll
// accept. This bounds the number of CRLs in the CRLSet as well as the number
// of serial numbers in a given CRL.
static const unsigned kMaxUncompressedChangesLength = 1024 * 1024;

static bool ReadChanges(base::StringPiece* data,
                        std::vector<uint8>* out_changes) {
  uint32 uncompressed_size, compressed_size;
  if (data->size() < 2 * sizeof(uint32))
    return false;
  // assumes little endian.
  memcpy(&uncompressed_size, data->data(), sizeof(uint32));
  data->remove_prefix(4);
  memcpy(&compressed_size, data->data(), sizeof(uint32));
  data->remove_prefix(4);

  if (uncompressed_size > kMaxUncompressedChangesLength)
    return false;
  if (data->size() < compressed_size)
    return false;

  out_changes->clear();
  if (uncompressed_size == 0)
    return true;

  out_changes->resize(uncompressed_size);
  base::StringPiece compressed(data->data(), compressed_size);
  data->remove_prefix(compressed_size);
  return DecompressZlib(&(*out_changes)[0], uncompressed_size, compressed);
}

// These are the range coder symbols used in delta updates.
enum {
  SYMBOL_SAME = 0,
  SYMBOL_INSERT = 1,
  SYMBOL_DELETE = 2,
  SYMBOL_CHANGED = 3,
};

bool ReadDeltaCRL(base::StringPiece* data,
                  const std::vector<std::string>& old_serials,
                  std::vector<std::string>* out_serials) {
  std::vector<uint8> changes;
  if (!ReadChanges(data, &changes))
    return false;

  size_t i = 0;
  for (std::vector<uint8>::const_iterator k = changes.begin();
       k != changes.end(); ++k) {
    if (*k == SYMBOL_SAME) {
      if (i >= old_serials.size())
        return false;
      out_serials->push_back(old_serials[i]);
      i++;
    } else if (*k == SYMBOL_INSERT) {
      uint8 serial_length;
      if (data->size() < sizeof(uint8))
        return false;
      memcpy(&serial_length, data->data(), sizeof(uint8));
      data->remove_prefix(sizeof(uint8));

      if (data->size() < serial_length)
        return false;
      const std::string serial(data->data(), serial_length);
      data->remove_prefix(serial_length);

      out_serials->push_back(serial);
    } else if (*k == SYMBOL_DELETE) {
      if (i >= old_serials.size())
        return false;
      i++;
    } else {
      NOTREACHED();
      return false;
    }
  }

  if (i != old_serials.size())
    return false;
  return true;
}

bool CRLSet::ApplyDelta(base::StringPiece data,
                        scoped_refptr<CRLSet>* out_crl_set) {
       scoped_ptr<DictionaryValue> header_dict(ReadHeader(&data));
  if (!header_dict.get())
    return false;

  std::string contents;
  if (!header_dict->GetString("ContentType", &contents))
    return false;
  if (contents != "CRLSetDelta")
    return false;

  int version;
  if (!header_dict->GetInteger("Version", &version) ||
      version != kCurrentFileVersion) {
    return false;
  }

  double next_update, update_window;
  int sequence, delta_from;
  if (!header_dict->GetDouble("NextUpdate", &next_update) ||
      !header_dict->GetDouble("WindowSecs", &update_window) ||
      !header_dict->GetInteger("Sequence", &sequence) ||
      !header_dict->GetInteger("DeltaFrom", &delta_from) ||
      delta_from < 0 ||
      static_cast<uint32>(delta_from) != sequence_) {
    return false;
  }

  scoped_refptr<CRLSet> crl_set(new CRLSet);
  crl_set->next_update_ = base::Time::FromDoubleT(next_update);
  crl_set->update_window_ =
      base::TimeDelta::FromSeconds(static_cast<int64>(update_window));
  crl_set->sequence_ = static_cast<uint32>(sequence);

  std::vector<uint8> crl_changes;

  if (!ReadChanges(&data, &crl_changes))
    return false;

  size_t i = 0, j = 0;
  for (std::vector<uint8>::const_iterator k = crl_changes.begin();
       k != crl_changes.end(); ++k) {
    if (*k == SYMBOL_SAME) {
      if (i >= crls_.size())
        return false;
      crl_set->crls_.push_back(crls_[i]);
      crl_set->crls_index_by_issuer_[crls_[i].first] = j;
      i++;
      j++;
    } else if (*k == SYMBOL_INSERT) {
      std::string parent_spki_hash;
      std::vector<std::string> serials;
      if (!ReadCRL(&data, &parent_spki_hash, &serials))
        return false;
      crl_set->crls_.push_back(std::make_pair(parent_spki_hash, serials));
      crl_set->crls_index_by_issuer_[parent_spki_hash] = j;
      j++;
    } else if (*k == SYMBOL_DELETE) {
      if (i >= crls_.size())
        return false;
      i++;
    } else if (*k == SYMBOL_CHANGED) {
      if (i >= crls_.size())
        return false;
      std::vector<std::string> serials;
      if (!ReadDeltaCRL(&data, crls_[i].second, &serials))
        return false;
      crl_set->crls_.push_back(std::make_pair(crls_[i].first, serials));
      crl_set->crls_index_by_issuer_[crls_[i].first] = j;
      i++;
      j++;
    } else {
      NOTREACHED();
      return false;
    }
  }

  if (!data.empty())
    return false;
  if (i != crls_.size())
    return false;

  *out_crl_set = crl_set;
  return true;
}

CRLSet::Result CRLSet::CheckCertificate(
    const base::StringPiece& serial_number,
    const base::StringPiece& parent_spki) const {
  std::map<std::string, size_t>::const_iterator i =
      crls_index_by_issuer_.find(parent_spki.as_string());
  if (i == crls_index_by_issuer_.end())
    return UNKNOWN;
  const std::vector<std::string>& serials = crls_[i->second].second;

  for (std::vector<std::string>::const_iterator i = serials.begin();
       i != serials.end(); ++i) {
    if (*i == serial_number.as_string())
      return REVOKED;
  }

  return GOOD;
}

base::Time CRLSet::next_update() const {
  return next_update_;
}

base::TimeDelta CRLSet::update_window() const {
  return update_window_;
}

uint32 CRLSet::sequence() const {
  return sequence_;
}

const CRLSet::CRLList& CRLSet::crls() const {
  return crls_;
}

}  // namespace net
