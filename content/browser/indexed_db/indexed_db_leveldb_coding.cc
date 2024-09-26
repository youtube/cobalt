// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"

#include <iterator>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

#include "base/big_endian.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_byteorder.h"
#include "build/build_config.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes_coding.h"
#include "components/services/storage/indexed_db/scopes/varint_coding.h"

// See leveldb_coding_scheme.md for detailed documentation of the coding
// scheme implemented here.

using base::StringPiece;
using blink::IndexedDBKey;
using blink::IndexedDBKeyPath;

namespace content {
namespace {
inline uint64_t ByteSwapToBE64(uint64_t x) {
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  return base::ByteSwap(x);
#else
  return x;
#endif
}

// As most of the IndexedDBKeys and encoded values are short, we
// initialize some std::vectors with a default inline buffer size to reduce
// the memory re-allocations when the std::vectors are appended.
const size_t kDefaultInlineBufferSize = 32;

constexpr unsigned char kIndexedDBKeyNullTypeByte = 0;
constexpr unsigned char kIndexedDBKeyStringTypeByte = 1;
constexpr unsigned char kIndexedDBKeyDateTypeByte = 2;
constexpr unsigned char kIndexedDBKeyNumberTypeByte = 3;
constexpr unsigned char kIndexedDBKeyArrayTypeByte = 4;
constexpr unsigned char kIndexedDBKeyMinKeyTypeByte = 5;
constexpr unsigned char kIndexedDBKeyBinaryTypeByte = 6;

constexpr unsigned char kIndexedDBKeyPathTypeCodedByte1 = 0;
constexpr unsigned char kIndexedDBKeyPathTypeCodedByte2 = 0;

constexpr unsigned char kIndexedDBKeyPathNullTypeByte = 0;
constexpr unsigned char kIndexedDBKeyPathStringTypeByte = 1;
constexpr unsigned char kIndexedDBKeyPathArrayTypeByte = 2;

constexpr unsigned char kObjectStoreDataIndexId = 1;
constexpr unsigned char kExistsEntryIndexId = 2;
constexpr unsigned char kBlobEntryIndexId = 3;

constexpr unsigned char kSchemaVersionTypeByte = 0;
constexpr unsigned char kMaxDatabaseIdTypeByte = 1;
constexpr unsigned char kDataVersionTypeByte = 2;
constexpr unsigned char kRecoveryBlobJournalTypeByte = 3;
constexpr unsigned char kActiveBlobJournalTypeByte = 4;
constexpr unsigned char kEarliestSweepTimeTypeByte = 5;
constexpr unsigned char kEarliestCompactionTimeTypeByte = 6;
constexpr unsigned char kMaxSimpleGlobalMetaDataTypeByte =
    7;  // Insert before this and increment.
constexpr unsigned char kScopesPrefixByte = 50;
constexpr unsigned char kDatabaseFreeListTypeByte = 100;
constexpr unsigned char kDatabaseNameTypeByte = 201;

constexpr unsigned char kObjectStoreMetaDataTypeByte = 50;
constexpr unsigned char kIndexMetaDataTypeByte = 100;
constexpr unsigned char kObjectStoreFreeListTypeByte = 150;
constexpr unsigned char kIndexFreeListTypeByte = 151;
constexpr unsigned char kObjectStoreNamesTypeByte = 200;
constexpr unsigned char kIndexNamesKeyTypeByte = 201;

constexpr unsigned char kObjectMetaDataTypeMaximum = 255;
constexpr unsigned char kIndexMetaDataTypeMaximum = 255;

const constexpr int kDatabaseLockPartition = 0;
const constexpr int kObjectStoreLockPartition = 1;

inline void EncodeIntSafely(int64_t value, int64_t max, std::string* into) {
  DCHECK_LE(value, max);
  return EncodeInt(value, into);
}

}  // namespace

const unsigned char kMinimumIndexId = 30;

std::string MaxIDBKey() {
  std::string ret;
  EncodeByte(kIndexedDBKeyNullTypeByte, &ret);
  return ret;
}

std::string MinIDBKey() {
  std::string ret;
  EncodeByte(kIndexedDBKeyMinKeyTypeByte, &ret);
  return ret;
}

void EncodeByte(unsigned char value, std::string* into) {
  into->push_back(value);
}

void EncodeBool(bool value, std::string* into) {
  into->push_back(value ? 1 : 0);
}

void EncodeInt(int64_t value, std::string* into) {
#ifndef NDEBUG
  // Exercised by unit tests in debug only.
  DCHECK_GE(value, 0);
#endif
  uint64_t n = static_cast<uint64_t>(value);

  do {
    unsigned char c = n;
    into->push_back(c);
    n >>= 8;
  } while (n);
}

void EncodeString(const std::u16string& value, std::string* into) {
  if (value.empty())
    return;
  // Backing store is UTF-16BE, convert from host endianness.
  size_t length = value.length();
  size_t current = into->size();
  into->resize(into->size() + length * sizeof(char16_t));

  const char16_t* src = value.c_str();
  char16_t* dst = reinterpret_cast<char16_t*>(&*into->begin() + current);
  for (unsigned i = 0; i < length; ++i)
    *dst++ = base::HostToNet16(*src++);
}

void EncodeBinary(const std::string& value, std::string* into) {
  EncodeVarInt(value.length(), into);
  into->append(value.begin(), value.end());
  DCHECK_GE(into->size(), value.size());
}

void EncodeBinary(base::span<const uint8_t> value, std::string* into) {
  EncodeVarInt(value.size(), into);
  into->append(value.begin(), value.end());
  DCHECK_GE(into->size(), value.size());
}

void EncodeStringWithLength(const std::u16string& value, std::string* into) {
  EncodeVarInt(value.length(), into);
  EncodeString(value, into);
}

void EncodeDouble(double value, std::string* into) {
  // This always has host endianness.
  const char* p = reinterpret_cast<char*>(&value);
  into->insert(into->end(), p, p + sizeof(value));
}

void EncodeIDBKey(const IndexedDBKey& value, std::string* into) {
  size_t previous_size = into->size();
  DCHECK(value.IsValid());
  switch (value.type()) {
    case blink::mojom::IDBKeyType::Array: {
      EncodeByte(kIndexedDBKeyArrayTypeByte, into);
      size_t length = value.array().size();
      EncodeVarInt(length, into);
      for (size_t i = 0; i < length; ++i)
        EncodeIDBKey(value.array()[i], into);
      DCHECK_GT(into->size(), previous_size);
      return;
    }
    case blink::mojom::IDBKeyType::Binary:
      EncodeByte(kIndexedDBKeyBinaryTypeByte, into);
      EncodeBinary(value.binary(), into);
      DCHECK_GT(into->size(), previous_size);
      return;
    case blink::mojom::IDBKeyType::String:
      EncodeByte(kIndexedDBKeyStringTypeByte, into);
      EncodeStringWithLength(value.string(), into);
      DCHECK_GT(into->size(), previous_size);
      return;
    case blink::mojom::IDBKeyType::Date:
      EncodeByte(kIndexedDBKeyDateTypeByte, into);
      EncodeDouble(value.date(), into);
      DCHECK_EQ(9u, static_cast<size_t>(into->size() - previous_size));
      return;
    case blink::mojom::IDBKeyType::Number:
      EncodeByte(kIndexedDBKeyNumberTypeByte, into);
      EncodeDouble(value.number(), into);
      DCHECK_EQ(9u, static_cast<size_t>(into->size() - previous_size));
      return;
    case blink::mojom::IDBKeyType::None:
    case blink::mojom::IDBKeyType::Invalid:
    case blink::mojom::IDBKeyType::Min:
    default:
      NOTREACHED();
      EncodeByte(kIndexedDBKeyNullTypeByte, into);
      return;
  }
}

#define COMPILE_ASSERT_MATCHING_VALUES(a, b)                          \
  static_assert(                                                      \
      static_cast<unsigned char>(a) == static_cast<unsigned char>(b), \
      "Blink enum and coding byte must match.")

COMPILE_ASSERT_MATCHING_VALUES(blink::mojom::IDBKeyPathType::Null,
                               kIndexedDBKeyPathNullTypeByte);
COMPILE_ASSERT_MATCHING_VALUES(blink::mojom::IDBKeyPathType::String,
                               kIndexedDBKeyPathStringTypeByte);
COMPILE_ASSERT_MATCHING_VALUES(blink::mojom::IDBKeyPathType::Array,
                               kIndexedDBKeyPathArrayTypeByte);

void EncodeIDBKeyPath(const IndexedDBKeyPath& value, std::string* into) {
  // May be typed, or may be a raw string. An invalid leading
  // byte is used to identify typed coding. New records are
  // always written as typed.
  EncodeByte(kIndexedDBKeyPathTypeCodedByte1, into);
  EncodeByte(kIndexedDBKeyPathTypeCodedByte2, into);
  EncodeByte(static_cast<char>(value.type()), into);
  switch (value.type()) {
    case blink::mojom::IDBKeyPathType::Null:
      break;
    case blink::mojom::IDBKeyPathType::String: {
      EncodeStringWithLength(value.string(), into);
      break;
    }
    case blink::mojom::IDBKeyPathType::Array: {
      const std::vector<std::u16string>& array = value.array();
      size_t count = array.size();
      EncodeVarInt(count, into);
      for (size_t i = 0; i < count; ++i) {
        EncodeStringWithLength(array[i], into);
      }
      break;
    }
  }
}

void EncodeBlobJournal(const BlobJournalType& journal, std::string* into) {
  for (const auto& iter : journal) {
    EncodeVarInt(iter.first, into);
    EncodeVarInt(iter.second, into);
  }
}

bool DecodeByte(StringPiece* slice, unsigned char* value) {
  if (slice->empty())
    return false;

  *value = (*slice)[0];
  slice->remove_prefix(1);
  return true;
}

bool DecodeBool(StringPiece* slice, bool* value) {
  if (slice->empty())
    return false;

  *value = !!(*slice)[0];
  slice->remove_prefix(1);
  return true;
}

bool DecodeInt(StringPiece* slice, int64_t* value) {
  if (slice->empty())
    return false;

  StringPiece::const_iterator it = slice->begin();
  int shift = 0;
  int64_t ret = 0;
  while (it != slice->end()) {
    unsigned char c = *it++;
    ret |= static_cast<int64_t>(c) << shift;
    shift += 8;
  }
  *value = ret;
  slice->remove_prefix(it - slice->begin());
  return true;
}

bool DecodeString(StringPiece* slice, std::u16string* value) {
  if (slice->empty()) {
    value->clear();
    return true;
  }

  // Backing store is UTF-16BE, convert to host endianness.
  DCHECK(!(slice->size() % sizeof(char16_t)));
  size_t length = slice->size() / sizeof(char16_t);
  std::u16string decoded;
  decoded.reserve(length);
  const char16_t* encoded = reinterpret_cast<const char16_t*>(slice->begin());
  for (unsigned i = 0; i < length; ++i)
    decoded.push_back(base::NetToHost16(*encoded++));

  *value = decoded;
  slice->remove_prefix(length * sizeof(char16_t));
  return true;
}

bool DecodeStringWithLength(StringPiece* slice, std::u16string* value) {
  if (slice->empty())
    return false;

  int64_t length = 0;
  if (!DecodeVarInt(slice, &length) || length < 0)
    return false;
  size_t bytes = length * sizeof(char16_t);
  if (slice->size() < bytes)
    return false;

  StringPiece subpiece(slice->begin(), bytes);
  slice->remove_prefix(bytes);
  if (!DecodeString(&subpiece, value))
    return false;

  return true;
}

bool DecodeBinary(StringPiece* slice, std::string* value) {
  if (slice->empty())
    return false;

  int64_t length = 0;
  if (!DecodeVarInt(slice, &length) || length < 0)
    return false;
  size_t size = length;
  if (slice->size() < size)
    return false;

  value->assign(slice->begin(), size);
  slice->remove_prefix(size);
  return true;
}

bool DecodeBinary(StringPiece* slice, base::span<const uint8_t>* value) {
  if (slice->empty())
    return false;

  int64_t length = 0;
  if (!DecodeVarInt(slice, &length) || length < 0)
    return false;
  size_t size = length;
  if (slice->size() < size)
    return false;

  *value = base::as_bytes(base::make_span(slice->substr(0, size)));
  slice->remove_prefix(size);
  return true;
}

bool DecodeIDBKeyRecursive(StringPiece* slice,
                           std::unique_ptr<IndexedDBKey>* value,
                           size_t recursion) {
  if (slice->empty())
    return false;

  if (recursion > IndexedDBKey::kMaximumDepth)
    return false;

  unsigned char type = (*slice)[0];
  slice->remove_prefix(1);

  switch (type) {
    case kIndexedDBKeyNullTypeByte:
      *value = std::make_unique<IndexedDBKey>();
      return true;

    case kIndexedDBKeyArrayTypeByte: {
      int64_t length = 0;
      if (!DecodeVarInt(slice, &length) || length < 0)
        return false;
      IndexedDBKey::KeyArray array;
      while (length--) {
        std::unique_ptr<IndexedDBKey> key;
        if (!DecodeIDBKeyRecursive(slice, &key, recursion + 1))
          return false;
        array.push_back(*key);
      }
      *value = std::make_unique<IndexedDBKey>(std::move(array));
      return true;
    }
    case kIndexedDBKeyBinaryTypeByte: {
      std::string binary;
      if (!DecodeBinary(slice, &binary))
        return false;
      *value = std::make_unique<IndexedDBKey>(std::move(binary));
      return true;
    }
    case kIndexedDBKeyStringTypeByte: {
      std::u16string s;
      if (!DecodeStringWithLength(slice, &s))
        return false;
      *value = std::make_unique<IndexedDBKey>(std::move(s));
      return true;
    }
    case kIndexedDBKeyDateTypeByte: {
      double d;
      if (!DecodeDouble(slice, &d))
        return false;
      *value =
          std::make_unique<IndexedDBKey>(d, blink::mojom::IDBKeyType::Date);
      return true;
    }
    case kIndexedDBKeyNumberTypeByte: {
      double d;
      if (!DecodeDouble(slice, &d))
        return false;
      *value =
          std::make_unique<IndexedDBKey>(d, blink::mojom::IDBKeyType::Number);
      return true;
    }
    case kIndexedDBKeyMinKeyTypeByte: {
      *value = std::make_unique<IndexedDBKey>(blink::mojom::IDBKeyType::Min);
      return true;
    }
  }

  NOTREACHED();
  return false;
}

bool DecodeIDBKey(StringPiece* slice, std::unique_ptr<IndexedDBKey>* value) {
  return DecodeIDBKeyRecursive(slice, value, 0);
}

bool DecodeDouble(StringPiece* slice, double* value) {
  if (slice->size() < sizeof(*value))
    return false;

  memcpy(value, slice->begin(), sizeof(*value));
  slice->remove_prefix(sizeof(*value));
  return true;
}

bool DecodeIDBKeyPath(StringPiece* slice, IndexedDBKeyPath* value) {
  // May be typed, or may be a raw string. An invalid leading
  // byte sequence is used to identify typed coding. New records are
  // always written as typed.
  if (slice->size() < 3 || (*slice)[0] != kIndexedDBKeyPathTypeCodedByte1 ||
      (*slice)[1] != kIndexedDBKeyPathTypeCodedByte2) {
    std::u16string s;
    if (!DecodeString(slice, &s))
      return false;
    *value = IndexedDBKeyPath(s);
    return true;
  }

  slice->remove_prefix(2);
  DCHECK(!slice->empty());
  blink::mojom::IDBKeyPathType type =
      static_cast<blink::mojom::IDBKeyPathType>((*slice)[0]);
  slice->remove_prefix(1);

  switch (type) {
    case blink::mojom::IDBKeyPathType::Null:
      DCHECK(slice->empty());
      *value = IndexedDBKeyPath();
      return true;
    case blink::mojom::IDBKeyPathType::String: {
      std::u16string string;
      if (!DecodeStringWithLength(slice, &string))
        return false;
      DCHECK(slice->empty());
      *value = IndexedDBKeyPath(string);
      return true;
    }
    case blink::mojom::IDBKeyPathType::Array: {
      std::vector<std::u16string> array;
      int64_t count;
      if (!DecodeVarInt(slice, &count) || count < 0)
        return false;
      while (count--) {
        std::u16string string;
        if (!DecodeStringWithLength(slice, &string))
          return false;
        array.push_back(string);
      }
      DCHECK(slice->empty());
      *value = IndexedDBKeyPath(array);
      return true;
    }
  }
  NOTREACHED();
  return false;
}

bool DecodeBlobJournal(StringPiece* slice, BlobJournalType* journal) {
  BlobJournalType output;
  while (!slice->empty()) {
    int64_t database_id = -1;
    int64_t blob_number = -1;
    if (!DecodeVarInt(slice, &database_id))
      return false;
    if (!KeyPrefix::IsValidDatabaseId(database_id))
      return false;
    if (!DecodeVarInt(slice, &blob_number))
      return false;
    if (!DatabaseMetaDataKey::IsValidBlobNumber(blob_number) &&
        (blob_number != DatabaseMetaDataKey::kAllBlobsNumber)) {
      return false;
    }
    output.push_back({database_id, blob_number});
  }
  journal->swap(output);
  return true;
}

bool ConsumeEncodedIDBKey(StringPiece* slice) {
  unsigned char type = (*slice)[0];
  slice->remove_prefix(1);

  switch (type) {
    case kIndexedDBKeyNullTypeByte:
    case kIndexedDBKeyMinKeyTypeByte:
      return true;
    case kIndexedDBKeyArrayTypeByte: {
      int64_t length;
      if (!DecodeVarInt(slice, &length) || length < 0)
        return false;
      while (length--) {
        if (!ConsumeEncodedIDBKey(slice))
          return false;
      }
      return true;
    }
    case kIndexedDBKeyBinaryTypeByte: {
      int64_t length = 0;
      if (!DecodeVarInt(slice, &length) || length < 0)
        return false;
      if (slice->size() < static_cast<size_t>(length))
        return false;
      slice->remove_prefix(length);
      return true;
    }
    case kIndexedDBKeyStringTypeByte: {
      int64_t length = 0;
      if (!DecodeVarInt(slice, &length) || length < 0)
        return false;
      if (slice->size() < static_cast<size_t>(length) * sizeof(char16_t))
        return false;
      slice->remove_prefix(length * sizeof(char16_t));
      return true;
    }
    case kIndexedDBKeyDateTypeByte:
    case kIndexedDBKeyNumberTypeByte:
      if (slice->size() < sizeof(double))
        return false;
      slice->remove_prefix(sizeof(double));
      return true;
  }
  NOTREACHED();
  return false;
}

bool ExtractEncodedIDBKey(StringPiece* slice, std::string* result) {
  const char* start = slice->begin();
  if (!ConsumeEncodedIDBKey(slice))
    return false;

  if (result)
    result->assign(start, slice->begin());
  return true;
}

static blink::mojom::IDBKeyType KeyTypeByteToKeyType(unsigned char type) {
  switch (type) {
    case kIndexedDBKeyNullTypeByte:
      return blink::mojom::IDBKeyType::Invalid;
    case kIndexedDBKeyArrayTypeByte:
      return blink::mojom::IDBKeyType::Array;
    case kIndexedDBKeyBinaryTypeByte:
      return blink::mojom::IDBKeyType::Binary;
    case kIndexedDBKeyStringTypeByte:
      return blink::mojom::IDBKeyType::String;
    case kIndexedDBKeyDateTypeByte:
      return blink::mojom::IDBKeyType::Date;
    case kIndexedDBKeyNumberTypeByte:
      return blink::mojom::IDBKeyType::Number;
    case kIndexedDBKeyMinKeyTypeByte:
      return blink::mojom::IDBKeyType::Min;
  }

  NOTREACHED() << "Got invalid type " << type;
  return blink::mojom::IDBKeyType::Invalid;
}

int CompareEncodedStringsWithLength(StringPiece* slice1,
                                    StringPiece* slice2,
                                    bool* ok) {
  int64_t len1, len2;
  if (!DecodeVarInt(slice1, &len1) || !DecodeVarInt(slice2, &len2)) {
    *ok = false;
    return 0;
  }
  if (len1 < 0 || len2 < 0) {
    *ok = false;
    return 0;
  }
  if (slice1->size() < len1 * sizeof(char16_t) ||
      slice2->size() < len2 * sizeof(char16_t)) {
    *ok = false;
    return 0;
  }

  // Extract the string data, and advance the passed slices.
  StringPiece string1(slice1->begin(), len1 * sizeof(char16_t));
  StringPiece string2(slice2->begin(), len2 * sizeof(char16_t));
  slice1->remove_prefix(len1 * sizeof(char16_t));
  slice2->remove_prefix(len2 * sizeof(char16_t));

  *ok = true;
  // Strings are UTF-16BE encoded, so a simple memcmp is sufficient.
  return string1.compare(string2);
}

int CompareEncodedBinary(StringPiece* slice1, StringPiece* slice2, bool* ok) {
  int64_t len1, len2;
  if (!DecodeVarInt(slice1, &len1) || !DecodeVarInt(slice2, &len2)) {
    *ok = false;
    return 0;
  }
  if (len1 < 0 || len2 < 0) {
    *ok = false;
    return 0;
  }
  size_t size1 = len1;
  size_t size2 = len2;

  if (slice1->size() < size1 || slice2->size() < size2) {
    *ok = false;
    return 0;
  }

  // Extract the binary data, and advance the passed slices.
  StringPiece binary1(slice1->begin(), size1);
  StringPiece binary2(slice2->begin(), size2);
  slice1->remove_prefix(size1);
  slice2->remove_prefix(size2);

  *ok = true;
  // This is the same as a memcmp()
  return binary1.compare(binary2);
}

static int CompareInts(int64_t a, int64_t b) {
#ifndef NDEBUG
  // Exercised by unit tests in debug only.
  DCHECK_GE(a, 0);
  DCHECK_GE(b, 0);
#endif
  int64_t diff = a - b;
  if (diff < 0)
    return -1;
  if (diff > 0)
    return 1;
  return 0;
}

static inline int CompareSizes(size_t a, size_t b) {
  if (a > b)
    return 1;
  if (b > a)
    return -1;
  return 0;
}

static int CompareTypes(blink::mojom::IDBKeyType a,
                        blink::mojom::IDBKeyType b) {
  return static_cast<int32_t>(b) - static_cast<int32_t>(a);
}

int CompareEncodedIDBKeys(StringPiece* slice_a,
                          StringPiece* slice_b,
                          bool* ok) {
  DCHECK(!slice_a->empty());
  DCHECK(!slice_b->empty());
  *ok = true;
  unsigned char type_a = (*slice_a)[0];
  unsigned char type_b = (*slice_b)[0];
  slice_a->remove_prefix(1);
  slice_b->remove_prefix(1);

  if (int x = CompareTypes(KeyTypeByteToKeyType(type_a),
                           KeyTypeByteToKeyType(type_b)))
    return x;

  switch (type_a) {
    case kIndexedDBKeyNullTypeByte:
    case kIndexedDBKeyMinKeyTypeByte:
      // Null type or max type; no payload to compare.
      return 0;
    case kIndexedDBKeyArrayTypeByte: {
      int64_t length_a, length_b;
      if (!DecodeVarInt(slice_a, &length_a) ||
          !DecodeVarInt(slice_b, &length_b)) {
        *ok = false;
        return 0;
      }
      for (int64_t i = 0; i < length_a && i < length_b; ++i) {
        int result = CompareEncodedIDBKeys(slice_a, slice_b, ok);
        if (!*ok || result)
          return result;
      }
      return length_a - length_b;
    }
    case kIndexedDBKeyBinaryTypeByte:
      return CompareEncodedBinary(slice_a, slice_b, ok);
    case kIndexedDBKeyStringTypeByte:
      return CompareEncodedStringsWithLength(slice_a, slice_b, ok);
    case kIndexedDBKeyDateTypeByte:
    case kIndexedDBKeyNumberTypeByte: {
      double d, e;
      if (!DecodeDouble(slice_a, &d) || !DecodeDouble(slice_b, &e)) {
        *ok = false;
        return 0;
      }
      if (d < e)
        return -1;
      if (d > e)
        return 1;
      return 0;
    }
  }

  NOTREACHED();
  return 0;
}

namespace {

template <typename KeyType>
int Compare(const StringPiece& a,
            const StringPiece& b,
            bool only_compare_index_keys,
            bool* ok) {
  KeyType key_a;
  KeyType key_b;

  StringPiece slice_a(a);
  if (!KeyType::Decode(&slice_a, &key_a)) {
    *ok = false;
    return 0;
  }
  StringPiece slice_b(b);
  if (!KeyType::Decode(&slice_b, &key_b)) {
    *ok = false;
    return 0;
  }

  *ok = true;
  return key_a.Compare(key_b);
}

template <typename KeyType>
int CompareSuffix(StringPiece* a,
                  StringPiece* b,
                  bool only_compare_index_keys,
                  bool* ok) {
  NOTREACHED();
  return 0;
}

template <>
int CompareSuffix<ExistsEntryKey>(StringPiece* slice_a,
                                  StringPiece* slice_b,
                                  bool only_compare_index_keys,
                                  bool* ok) {
  DCHECK(!slice_a->empty());
  DCHECK(!slice_b->empty());
  return CompareEncodedIDBKeys(slice_a, slice_b, ok);
}

template <>
int CompareSuffix<ObjectStoreDataKey>(StringPiece* slice_a,
                                      StringPiece* slice_b,
                                      bool only_compare_index_keys,
                                      bool* ok) {
  return CompareEncodedIDBKeys(slice_a, slice_b, ok);
}

template <>
int CompareSuffix<BlobEntryKey>(StringPiece* slice_a,
                                StringPiece* slice_b,
                                bool only_compare_index_keys,
                                bool* ok) {
  return CompareEncodedIDBKeys(slice_a, slice_b, ok);
}

template <>
int CompareSuffix<IndexDataKey>(StringPiece* slice_a,
                                StringPiece* slice_b,
                                bool only_compare_index_keys,
                                bool* ok) {
  // index key
  int result = CompareEncodedIDBKeys(slice_a, slice_b, ok);
  if (!*ok || result)
    return result;
  if (only_compare_index_keys)
    return 0;

  // sequence number [optional]
  int64_t sequence_number_a = -1;
  int64_t sequence_number_b = -1;
  if (!slice_a->empty() && !DecodeVarInt(slice_a, &sequence_number_a))
    return 0;
  if (!slice_b->empty() && !DecodeVarInt(slice_b, &sequence_number_b))
    return 0;

  if (slice_a->empty() || slice_b->empty())
    return CompareSizes(slice_a->size(), slice_b->size());

  // primary key [optional]
  result = CompareEncodedIDBKeys(slice_a, slice_b, ok);
  if (!*ok || result)
    return result;

  return CompareInts(sequence_number_a, sequence_number_b);
}

int Compare(const StringPiece& a,
            const StringPiece& b,
            bool only_compare_index_keys,
            bool* ok) {
  StringPiece slice_a(a);
  StringPiece slice_b(b);
  KeyPrefix prefix_a;
  KeyPrefix prefix_b;
  bool ok_a = KeyPrefix::Decode(&slice_a, &prefix_a);
  bool ok_b = KeyPrefix::Decode(&slice_b, &prefix_b);
  if (!ok_a || !ok_b) {
    *ok = false;
    return 0;
  }

  *ok = true;
  if (int x = prefix_a.Compare(prefix_b))
    return x;

  switch (prefix_a.type()) {
    case KeyPrefix::GLOBAL_METADATA: {
      DCHECK(!slice_a.empty());
      DCHECK(!slice_b.empty());

      unsigned char type_byte_a;
      if (!DecodeByte(&slice_a, &type_byte_a)) {
        *ok = false;
        return 0;
      }

      unsigned char type_byte_b;
      if (!DecodeByte(&slice_b, &type_byte_b)) {
        *ok = false;
        return 0;
      }

      if (int x = type_byte_a - type_byte_b)
        return x;
      if (type_byte_a < kMaxSimpleGlobalMetaDataTypeByte)
        return 0;

      if (type_byte_a == kScopesPrefixByte)
        return slice_a.compare(slice_b);

      // Compare<> is used (which re-decodes the prefix) rather than an
      // specialized CompareSuffix<> because metadata is relatively uncommon
      // in the database.

      if (type_byte_a == kDatabaseFreeListTypeByte) {
        // TODO(jsbell): No need to pass only_compare_index_keys through here.
        return Compare<DatabaseFreeListKey>(a, b, only_compare_index_keys, ok);
      }
      if (type_byte_a == kDatabaseNameTypeByte) {
        return Compare<DatabaseNameKey>(a, b, /*only_compare_index_keys*/ false,
                                        ok);
      }
      break;
    }

    case KeyPrefix::DATABASE_METADATA: {
      DCHECK(!slice_a.empty());
      DCHECK(!slice_b.empty());

      unsigned char type_byte_a;
      if (!DecodeByte(&slice_a, &type_byte_a)) {
        *ok = false;
        return 0;
      }

      unsigned char type_byte_b;
      if (!DecodeByte(&slice_b, &type_byte_b)) {
        *ok = false;
        return 0;
      }

      if (int x = type_byte_a - type_byte_b)
        return x;
      if (type_byte_a < DatabaseMetaDataKey::MAX_SIMPLE_METADATA_TYPE)
        return 0;

      // Compare<> is used (which re-decodes the prefix) rather than an
      // specialized CompareSuffix<> because metadata is relatively uncommon
      // in the database.

      if (type_byte_a == kObjectStoreMetaDataTypeByte) {
        // TODO(jsbell): No need to pass only_compare_index_keys through here.
        return Compare<ObjectStoreMetaDataKey>(a, b, only_compare_index_keys,
                                               ok);
      }
      if (type_byte_a == kIndexMetaDataTypeByte) {
        return Compare<IndexMetaDataKey>(a, b,
                                         /*only_compare_index_keys*/ false, ok);
      }
      if (type_byte_a == kObjectStoreFreeListTypeByte) {
        return Compare<ObjectStoreFreeListKey>(a, b, only_compare_index_keys,
                                               ok);
      }
      if (type_byte_a == kIndexFreeListTypeByte) {
        return Compare<IndexFreeListKey>(a, b,
                                         /*only_compare_index_keys*/ false, ok);
      }
      if (type_byte_a == kObjectStoreNamesTypeByte) {
        // TODO(jsbell): No need to pass only_compare_index_keys through here.
        return Compare<ObjectStoreNamesKey>(a, b, only_compare_index_keys, ok);
      }
      if (type_byte_a == kIndexNamesKeyTypeByte) {
        return Compare<IndexNamesKey>(a, b, /*only_compare_index_keys*/ false,
                                      ok);
      }
      break;
    }

    case KeyPrefix::OBJECT_STORE_DATA: {
      // Provide a stable ordering for invalid data.
      if (slice_a.empty() || slice_b.empty())
        return CompareSizes(slice_a.size(), slice_b.size());

      return CompareSuffix<ObjectStoreDataKey>(
          &slice_a, &slice_b, /*only_compare_index_keys*/ false, ok);
    }

    case KeyPrefix::EXISTS_ENTRY: {
      // Provide a stable ordering for invalid data.
      if (slice_a.empty() || slice_b.empty())
        return CompareSizes(slice_a.size(), slice_b.size());

      return CompareSuffix<ExistsEntryKey>(
          &slice_a, &slice_b, /*only_compare_index_keys*/ false, ok);
    }

    case KeyPrefix::BLOB_ENTRY: {
      // Provide a stable ordering for invalid data.
      if (slice_a.empty() || slice_b.empty())
        return CompareSizes(slice_a.size(), slice_b.size());

      return CompareSuffix<BlobEntryKey>(&slice_a, &slice_b,
                                         /*only_compare_index_keys*/ false, ok);
    }

    case KeyPrefix::INDEX_DATA: {
      // Provide a stable ordering for invalid data.
      if (slice_a.empty() || slice_b.empty())
        return CompareSizes(slice_a.size(), slice_b.size());

      return CompareSuffix<IndexDataKey>(&slice_a, &slice_b,
                                         only_compare_index_keys, ok);
    }

    case KeyPrefix::INVALID_TYPE:
      break;
  }

  NOTREACHED();
  *ok = false;
  return 0;
}

}  // namespace

int Compare(const StringPiece& a,
            const StringPiece& b,
            bool only_compare_index_keys) {
  bool ok;
  int result = Compare(a, b, only_compare_index_keys, &ok);
  // TODO(dmurph): Report this somehow. https://crbug.com/913121
  DCHECK(ok);
  if (!ok)
    return 0;
  return result;
}

int CompareKeys(const StringPiece& a, const StringPiece& b) {
  return Compare(a, b, /*index_keys=*/false);
}

int CompareIndexKeys(const StringPiece& a, const StringPiece& b) {
  return Compare(a, b, /*index_keys=*/true);
}

std::string IndexedDBKeyToDebugString(base::StringPiece key) {
  base::StringPiece key_with_prefix_preserved = key;
  KeyPrefix prefix;
  std::stringstream result;
  if (!KeyPrefix::Decode(&key, &prefix)) {
    result << "<Error decoding key prefix>";
    return result.str();
  }
  result << prefix.DebugString() << ", ";

  switch (prefix.type()) {
    case KeyPrefix::GLOBAL_METADATA: {
      unsigned char type_byte;
      if (!DecodeByte(&key, &type_byte)) {
        result << "No_Type_Byte";
        break;
      }
      switch (type_byte) {
        case kSchemaVersionTypeByte:
          result << "kSchemaVersionTypeByte";
          break;
        case kMaxDatabaseIdTypeByte:
          result << "kMaxDatabaseIdTypeByte";
          break;
        case kDataVersionTypeByte:
          result << "kDataVersionTypeByte";
          break;
        case kRecoveryBlobJournalTypeByte:
          result << "kRecoveryBlobJournalTypeByte";
          break;
        case kActiveBlobJournalTypeByte:
          result << "kActiveBlobJournalTypeByte";
          break;
        case kEarliestSweepTimeTypeByte:
          result << "kEarliestSweepTimeTypeByte";
          break;
        case kEarliestCompactionTimeTypeByte:
          result << "kEarliestCompactionTimeTypeByte";
          break;
        case kScopesPrefixByte:
          result << "Scopes key: "
                 << leveldb_scopes::KeyToDebugString(base::make_span(
                        reinterpret_cast<const uint8_t*>(key.data()),
                        key.size()));
          break;
        case kDatabaseFreeListTypeByte: {
          DatabaseFreeListKey db_free_list_key;
          if (!DatabaseFreeListKey::Decode(&key_with_prefix_preserved,
                                           &db_free_list_key)) {
            result << "kDatabaseFreeListTypeByte, Invalid_Key";
            break;
          }
          result << db_free_list_key.DebugString();
          break;
        }
        case kDatabaseNameTypeByte: {
          DatabaseNameKey db_name_key;
          if (!DatabaseNameKey::Decode(&key_with_prefix_preserved,
                                       &db_name_key)) {
            result << "kDatabaseNameTypeByte, Invalid_Key";
            break;
          }
          result << db_name_key.DebugString();
          break;
        }
        default:
          result << "Invalid_metadata_type";
          break;
      }
      break;
    }

    case KeyPrefix::DATABASE_METADATA: {
      unsigned char type_byte;
      if (!DecodeByte(&key, &type_byte)) {
        result << "No_Type_Byte";
        break;
      }
      switch (type_byte) {
        case DatabaseMetaDataKey::ORIGIN_NAME:
          result << "ORIGIN_NAME";
          break;
        case DatabaseMetaDataKey::DATABASE_NAME:
          result << "DATABASE_NAME";
          break;
        case DatabaseMetaDataKey::USER_STRING_VERSION:
          result << "USER_STRING_VERSION";
          break;
        case DatabaseMetaDataKey::MAX_OBJECT_STORE_ID:
          result << "MAX_OBJECT_STORE_ID";
          break;
        case DatabaseMetaDataKey::USER_VERSION:
          result << "USER_VERSION";
          break;
        case DatabaseMetaDataKey::BLOB_KEY_GENERATOR_CURRENT_NUMBER:
          result << "BLOB_KEY_GENERATOR_CURRENT_NUMBER";
          break;
        case kObjectStoreMetaDataTypeByte: {
          ObjectStoreMetaDataKey sub_key;
          if (!ObjectStoreMetaDataKey::Decode(&key_with_prefix_preserved,
                                              &sub_key)) {
            result << "Invalid_ObjectStoreMetaDataKey";
            break;
          }
          result << sub_key.DebugString();
          break;
        }
        case kIndexMetaDataTypeByte: {
          IndexMetaDataKey sub_key;
          if (!IndexMetaDataKey::Decode(&key_with_prefix_preserved, &sub_key)) {
            result << "Invalid_IndexMetaDataKey";
            break;
          }
          result << sub_key.DebugString();
          break;
        }
        case kObjectStoreFreeListTypeByte: {
          ObjectStoreFreeListKey sub_key;
          if (!ObjectStoreFreeListKey::Decode(&key_with_prefix_preserved,
                                              &sub_key)) {
            result << "Invalid_ObjectStoreFreeListKey";
            break;
          }
          result << sub_key.DebugString();
          break;
        }
        case kIndexFreeListTypeByte: {
          IndexFreeListKey sub_key;
          if (!IndexFreeListKey::Decode(&key_with_prefix_preserved, &sub_key)) {
            result << "Invalid_IndexFreeListKey";
            break;
          }
          result << sub_key.DebugString();
          break;
        }
        case kObjectStoreNamesTypeByte: {
          ObjectStoreNamesKey sub_key;
          if (!ObjectStoreNamesKey::Decode(&key_with_prefix_preserved,
                                           &sub_key)) {
            result << "Invalid_ObjectStoreNamesKey";
            break;
          }
          result << sub_key.DebugString();
          break;
        }  // namespace content
        case kIndexNamesKeyTypeByte: {
          IndexNamesKey sub_key;
          if (!IndexNamesKey::Decode(&key_with_prefix_preserved, &sub_key)) {
            result << "Invalid_IndexNamesKey";
            break;
          }
          result << sub_key.DebugString();
          break;
        }
      }
      break;
    }
    case KeyPrefix::OBJECT_STORE_DATA: {
      ObjectStoreDataKey sub_key;
      if (!ObjectStoreDataKey::Decode(&key_with_prefix_preserved, &sub_key)) {
        result << "Invalid_ObjectStoreDataKey";
        break;
      }
      result << sub_key.DebugString();
      break;
    }

    case KeyPrefix::EXISTS_ENTRY: {
      ExistsEntryKey sub_key;
      if (!ExistsEntryKey::Decode(&key_with_prefix_preserved, &sub_key)) {
        result << "Invalid_ExistsEntryKey";
        break;
      }
      result << sub_key.DebugString();
      break;
    }

    case KeyPrefix::BLOB_ENTRY: {
      BlobEntryKey sub_key;
      if (!BlobEntryKey::Decode(&key_with_prefix_preserved, &sub_key)) {
        result << "Invalid_BlobEntryKey";
        break;
      }
      result << sub_key.DebugString();
      break;
    }

    case KeyPrefix::INDEX_DATA: {
      IndexDataKey sub_key;
      if (!IndexDataKey::Decode(&key_with_prefix_preserved, &sub_key)) {
        result << "Invalid_IndexDataKey";
        break;
      }
      result << sub_key.DebugString();
      break;
    }

    case KeyPrefix::INVALID_TYPE:
      result << "InvalidKeyType";
      break;
  }
  result << "]";
  return result.str();
}

PartitionedLockId GetDatabaseLockId(int64_t database_id) {
  // These keys used to attempt to be bytewise-comparable, which is why
  // it uses big-endian encoding here. There was a goal to match the
  // existing leveldb key scheme used by IndexedDB. This is no longer a goal.
  uint64_t key[1] = {ByteSwapToBE64(static_cast<uint64_t>(database_id))};
  return {kDatabaseLockPartition,
          std::string(reinterpret_cast<char*>(&key), sizeof(key))};
}

PartitionedLockId GetObjectStoreLockId(int64_t database_id,
                                       int64_t object_store_id) {
  // These keys used to attempt to be bytewise-comparable, which is why
  // it uses big-endian encoding here. There was a goal to match the
  // existing leveldb key scheme used by IndexedDB. This is no longer a goal.
  uint64_t key[2] = {ByteSwapToBE64(static_cast<uint64_t>(database_id)),
                     ByteSwapToBE64(static_cast<uint64_t>(object_store_id))};
  return {kObjectStoreLockPartition,
          std::string(reinterpret_cast<char*>(&key), sizeof(key))};
}

KeyPrefix::KeyPrefix()
    : database_id_(INVALID_TYPE),
      object_store_id_(INVALID_TYPE),
      index_id_(INVALID_TYPE) {}

KeyPrefix::KeyPrefix(int64_t database_id)
    : database_id_(database_id), object_store_id_(0), index_id_(0) {
  DCHECK(KeyPrefix::IsValidDatabaseId(database_id));
}

KeyPrefix::KeyPrefix(int64_t database_id, int64_t object_store_id)
    : database_id_(database_id),
      object_store_id_(object_store_id),
      index_id_(0) {
  DCHECK(KeyPrefix::IsValidDatabaseId(database_id));
  DCHECK(KeyPrefix::IsValidObjectStoreId(object_store_id));
}

KeyPrefix::KeyPrefix(int64_t database_id,
                     int64_t object_store_id,
                     int64_t index_id)
    : database_id_(database_id),
      object_store_id_(object_store_id),
      index_id_(index_id) {
  DCHECK(KeyPrefix::IsValidDatabaseId(database_id));
  DCHECK(KeyPrefix::IsValidObjectStoreId(object_store_id));
  DCHECK(KeyPrefix::IsValidIndexId(index_id));
}

KeyPrefix::KeyPrefix(enum Type type,
                     int64_t database_id,
                     int64_t object_store_id,
                     int64_t index_id)
    : database_id_(database_id),
      object_store_id_(object_store_id),
      index_id_(index_id) {
  DCHECK_EQ(type, INVALID_TYPE);
  DCHECK(KeyPrefix::IsValidDatabaseId(database_id));
  DCHECK(KeyPrefix::IsValidObjectStoreId(object_store_id));
}

KeyPrefix KeyPrefix::CreateWithSpecialIndex(int64_t database_id,
                                            int64_t object_store_id,
                                            int64_t index_id) {
  DCHECK(KeyPrefix::IsValidDatabaseId(database_id));
  DCHECK(KeyPrefix::IsValidObjectStoreId(object_store_id));
  DCHECK(index_id);
  return KeyPrefix(INVALID_TYPE, database_id, object_store_id, index_id);
}

bool KeyPrefix::IsValidDatabaseId(int64_t database_id) {
  return (database_id > 0) && (database_id < KeyPrefix::kMaxDatabaseId);
}

bool KeyPrefix::IsValidObjectStoreId(int64_t object_store_id) {
  return (object_store_id > 0) &&
         (object_store_id < KeyPrefix::kMaxObjectStoreId);
}

bool KeyPrefix::IsValidIndexId(int64_t index_id) {
  return (index_id >= kMinimumIndexId) && (index_id < KeyPrefix::kMaxIndexId);
}

bool KeyPrefix::Decode(StringPiece* slice, KeyPrefix* result) {
  unsigned char first_byte;
  if (!DecodeByte(slice, &first_byte))
    return false;

  size_t database_id_bytes = ((first_byte >> 5) & 0x7) + 1;
  size_t object_store_id_bytes = ((first_byte >> 2) & 0x7) + 1;
  size_t index_id_bytes = (first_byte & 0x3) + 1;

  if (database_id_bytes + object_store_id_bytes + index_id_bytes >
      slice->size())
    return false;

  {
    StringPiece tmp(slice->begin(), database_id_bytes);
    if (!DecodeInt(&tmp, &result->database_id_))
      return false;
  }
  slice->remove_prefix(database_id_bytes);
  {
    StringPiece tmp(slice->begin(), object_store_id_bytes);
    if (!DecodeInt(&tmp, &result->object_store_id_))
      return false;
  }
  slice->remove_prefix(object_store_id_bytes);
  {
    StringPiece tmp(slice->begin(), index_id_bytes);
    if (!DecodeInt(&tmp, &result->index_id_))
      return false;
  }
  slice->remove_prefix(index_id_bytes);
  return true;
}

std::string KeyPrefix::EncodeEmpty() {
  const std::string result(4, 0);
  DCHECK_EQ(EncodeInternal(0, 0, 0), std::string(4, 0));
  return result;
}

std::string KeyPrefix::Encode() const {
  DCHECK_NE(database_id_, kInvalidId);
  DCHECK_NE(object_store_id_, kInvalidId);
  DCHECK_NE(index_id_, kInvalidId);
  return EncodeInternal(database_id_, object_store_id_, index_id_);
}

std::string KeyPrefix::EncodeInternal(int64_t database_id,
                                      int64_t object_store_id,
                                      int64_t index_id) {
  std::string database_id_string;
  std::string object_store_id_string;
  std::string index_id_string;

  EncodeIntSafely(database_id, kMaxDatabaseId, &database_id_string);
  EncodeIntSafely(object_store_id, kMaxObjectStoreId, &object_store_id_string);
  EncodeIntSafely(index_id, kMaxIndexId, &index_id_string);

  DCHECK_LE(database_id_string.size(), kMaxDatabaseIdSizeBytes);
  DCHECK_LE(object_store_id_string.size(), kMaxObjectStoreIdSizeBytes);
  DCHECK_LE(index_id_string.size(), kMaxIndexIdSizeBytes);

  unsigned char first_byte =
      (database_id_string.size() - 1)
          << (kMaxObjectStoreIdSizeBits + kMaxIndexIdSizeBits) |
      (object_store_id_string.size() - 1) << kMaxIndexIdSizeBits |
      (index_id_string.size() - 1);
  static_assert(kMaxDatabaseIdSizeBits + kMaxObjectStoreIdSizeBits +
                        kMaxIndexIdSizeBits ==
                    sizeof(first_byte) * 8,
                "cannot encode ids");
  std::string ret;
  ret.reserve(kDefaultInlineBufferSize);
  ret.push_back(first_byte);
  ret.append(database_id_string);
  ret.append(object_store_id_string);
  ret.append(index_id_string);

  DCHECK_LE(ret.size(), kDefaultInlineBufferSize);
  return ret;
}

int KeyPrefix::Compare(const KeyPrefix& other) const {
  DCHECK_NE(database_id_, kInvalidId);
  DCHECK_NE(object_store_id_, kInvalidId);
  DCHECK_NE(index_id_, kInvalidId);

  if (database_id_ != other.database_id_)
    return CompareInts(database_id_, other.database_id_);
  if (object_store_id_ != other.object_store_id_)
    return CompareInts(object_store_id_, other.object_store_id_);
  if (index_id_ != other.index_id_)
    return CompareInts(index_id_, other.index_id_);
  return 0;
}

std::string KeyPrefix::DebugString() {
  std::stringstream result;
  result << "{";
  switch (type()) {
    case GLOBAL_METADATA:
      result << "GLOBAL_META";
      break;
    case DATABASE_METADATA:
      result << "DB_META, db: " << database_id_;
      break;
    case OBJECT_STORE_DATA:
      result << "OS_DATA, db: " << database_id_ << ", os: " << object_store_id_;
      break;
    case EXISTS_ENTRY:
      result << "EXISTS_ENTRY, db: " << database_id_
             << ", os: " << object_store_id_;
      break;
    case BLOB_ENTRY:
      result << "BLOB_ENTRY, db: " << database_id_
             << ", os: " << object_store_id_;
      break;
    case INDEX_DATA:
      result << "INDEX_DATA, db: " << database_id_
             << ", os: " << object_store_id_ << ", idx: " << index_id_;
      break;
    case INVALID_TYPE:
      result << "INVALID_TYPE";
      break;
  }
  result << "}";
  return result.str();
}

KeyPrefix::Type KeyPrefix::type() const {
  DCHECK_NE(database_id_, kInvalidId);
  DCHECK_NE(object_store_id_, kInvalidId);
  DCHECK_NE(index_id_, kInvalidId);

  if (!database_id_)
    return GLOBAL_METADATA;
  if (!object_store_id_)
    return DATABASE_METADATA;
  if (index_id_ == kObjectStoreDataIndexId)
    return OBJECT_STORE_DATA;
  if (index_id_ == kExistsEntryIndexId)
    return EXISTS_ENTRY;
  if (index_id_ == kBlobEntryIndexId)
    return BLOB_ENTRY;
  if (index_id_ >= kMinimumIndexId)
    return INDEX_DATA;

  NOTREACHED();
  return INVALID_TYPE;
}

std::string SchemaVersionKey::Encode() {
  std::string ret = KeyPrefix::EncodeEmpty();
  ret.push_back(kSchemaVersionTypeByte);
  return ret;
}

std::string MaxDatabaseIdKey::Encode() {
  std::string ret = KeyPrefix::EncodeEmpty();
  ret.push_back(kMaxDatabaseIdTypeByte);
  return ret;
}

std::string DataVersionKey::Encode() {
  std::string ret = KeyPrefix::EncodeEmpty();
  ret.push_back(kDataVersionTypeByte);
  return ret;
}

std::string RecoveryBlobJournalKey::Encode() {
  std::string ret = KeyPrefix::EncodeEmpty();
  ret.push_back(kRecoveryBlobJournalTypeByte);
  return ret;
}

std::string ActiveBlobJournalKey::Encode() {
  std::string ret = KeyPrefix::EncodeEmpty();
  ret.push_back(kActiveBlobJournalTypeByte);
  return ret;
}

std::string EarliestSweepKey::Encode() {
  std::string ret = KeyPrefix::EncodeEmpty();
  ret.push_back(kEarliestSweepTimeTypeByte);
  return ret;
}

std::string EarliestCompactionKey::Encode() {
  std::string ret = KeyPrefix::EncodeEmpty();
  ret.push_back(kEarliestCompactionTimeTypeByte);
  return ret;
}

std::vector<uint8_t> ScopesPrefix::Encode() {
  std::string ret = KeyPrefix::EncodeEmpty();
  ret.push_back(kScopesPrefixByte);
  auto span = base::make_span(ret);
  return std::vector<uint8_t>(span.begin(), span.end());
}

DatabaseFreeListKey::DatabaseFreeListKey() : database_id_(-1) {}

bool DatabaseFreeListKey::Decode(StringPiece* slice,
                                 DatabaseFreeListKey* result) {
  KeyPrefix prefix;
  if (!KeyPrefix::Decode(slice, &prefix))
    return false;
  DCHECK(!prefix.database_id_);
  DCHECK(!prefix.object_store_id_);
  DCHECK(!prefix.index_id_);
  unsigned char type_byte = 0;
  if (!DecodeByte(slice, &type_byte))
    return false;
  DCHECK_EQ(type_byte, kDatabaseFreeListTypeByte);
  if (!DecodeVarInt(slice, &result->database_id_))
    return false;
  return true;
}

std::string DatabaseFreeListKey::Encode(int64_t database_id) {
  std::string ret = KeyPrefix::EncodeEmpty();
  ret.push_back(kDatabaseFreeListTypeByte);
  EncodeVarInt(database_id, &ret);
  return ret;
}

std::string DatabaseFreeListKey::EncodeMaxKey() {
  return Encode(std::numeric_limits<int64_t>::max());
}

int64_t DatabaseFreeListKey::DatabaseId() const {
  DCHECK_GE(database_id_, 0);
  return database_id_;
}

int DatabaseFreeListKey::Compare(const DatabaseFreeListKey& other) const {
  DCHECK_GE(database_id_, 0);
  return CompareInts(database_id_, other.database_id_);
}

std::string DatabaseFreeListKey::DebugString() const {
  std::stringstream result;
  result << "DatabaseFreeListKey{db: " << database_id_ << "}";
  return result.str();
}

bool DatabaseNameKey::Decode(StringPiece* slice, DatabaseNameKey* result) {
  KeyPrefix prefix;
  if (!KeyPrefix::Decode(slice, &prefix))
    return false;
  DCHECK(!prefix.database_id_);
  DCHECK(!prefix.object_store_id_);
  DCHECK(!prefix.index_id_);
  unsigned char type_byte = 0;
  if (!DecodeByte(slice, &type_byte))
    return false;
  DCHECK_EQ(type_byte, kDatabaseNameTypeByte);
  if (!DecodeStringWithLength(slice, &result->origin_))
    return false;
  if (!DecodeStringWithLength(slice, &result->database_name_))
    return false;
  return true;
}

std::string DatabaseNameKey::Encode(const std::string& origin_identifier,
                                    const std::u16string& database_name) {
  std::string ret = KeyPrefix::EncodeEmpty();
  ret.push_back(kDatabaseNameTypeByte);
  EncodeStringWithLength(base::ASCIIToUTF16(origin_identifier), &ret);
  EncodeStringWithLength(database_name, &ret);
  return ret;
}

std::string DatabaseNameKey::EncodeMinKeyForOrigin(
    const std::string& origin_identifier) {
  return Encode(origin_identifier, std::u16string());
}

std::string DatabaseNameKey::EncodeStopKeyForOrigin(
    const std::string& origin_identifier) {
  // just after origin in collation order
  return EncodeMinKeyForOrigin(origin_identifier + '\x01');
}

int DatabaseNameKey::Compare(const DatabaseNameKey& other) {
  if (int x = origin_.compare(other.origin_))
    return x;
  return database_name_.compare(other.database_name_);
}

std::string DatabaseNameKey::DebugString() const {
  std::stringstream result;
  result << "DatabaseNameKey{origin: " << origin_
         << ", database_name: " << database_name_ << "}";
  return result.str();
}

bool DatabaseMetaDataKey::IsValidBlobNumber(int64_t blob_number) {
  return blob_number >= kBlobNumberGeneratorInitialNumber;
}

const int64_t DatabaseMetaDataKey::kAllBlobsNumber = 1;
const int64_t DatabaseMetaDataKey::kBlobNumberGeneratorInitialNumber = 2;
const int64_t DatabaseMetaDataKey::kInvalidBlobNumber = -1;

std::string DatabaseMetaDataKey::Encode(int64_t database_id,
                                        MetaDataType meta_data_type) {
  KeyPrefix prefix(database_id);
  std::string ret = prefix.Encode();
  ret.push_back(meta_data_type);
  return ret;
}

const int64_t ObjectStoreMetaDataKey::kKeyGeneratorInitialNumber = 1;

ObjectStoreMetaDataKey::ObjectStoreMetaDataKey()
    : object_store_id_(-1), meta_data_type_(0xFF) {}

bool ObjectStoreMetaDataKey::Decode(StringPiece* slice,
                                    ObjectStoreMetaDataKey* result) {
  KeyPrefix prefix;
  if (!KeyPrefix::Decode(slice, &prefix))
    return false;
  DCHECK(prefix.database_id_);
  DCHECK(!prefix.object_store_id_);
  DCHECK(!prefix.index_id_);
  unsigned char type_byte = 0;
  if (!DecodeByte(slice, &type_byte))
    return false;
  DCHECK_EQ(type_byte, kObjectStoreMetaDataTypeByte);
  if (!DecodeVarInt(slice, &result->object_store_id_))
    return false;
  DCHECK(result->object_store_id_);
  if (!DecodeByte(slice, &result->meta_data_type_))
    return false;
  return true;
}

std::string ObjectStoreMetaDataKey::Encode(int64_t database_id,
                                           int64_t object_store_id,
                                           unsigned char meta_data_type) {
  KeyPrefix prefix(database_id);
  std::string ret = prefix.Encode();
  ret.push_back(kObjectStoreMetaDataTypeByte);
  EncodeVarInt(object_store_id, &ret);
  ret.push_back(meta_data_type);
  return ret;
}

std::string ObjectStoreMetaDataKey::EncodeMaxKey(int64_t database_id) {
  return Encode(database_id, std::numeric_limits<int64_t>::max(),
                kObjectMetaDataTypeMaximum);
}

std::string ObjectStoreMetaDataKey::EncodeMaxKey(int64_t database_id,
                                                 int64_t object_store_id) {
  return Encode(database_id, object_store_id, kObjectMetaDataTypeMaximum);
}

int64_t ObjectStoreMetaDataKey::ObjectStoreId() const {
  DCHECK_GE(object_store_id_, 0);
  return object_store_id_;
}
unsigned char ObjectStoreMetaDataKey::MetaDataType() const {
  return meta_data_type_;
}

int ObjectStoreMetaDataKey::Compare(const ObjectStoreMetaDataKey& other) {
  DCHECK_GE(object_store_id_, 0);
  if (int x = CompareInts(object_store_id_, other.object_store_id_))
    return x;
  return meta_data_type_ - other.meta_data_type_;
}

std::string ObjectStoreMetaDataKey::DebugString() const {
  std::stringstream result;
  result << "ObjectStoreMetaDataKey{os: " << object_store_id_;
  switch (meta_data_type_) {
    case NAME:
      result << ", NAME";
      break;
    case KEY_PATH:
      result << ", KEY_PATH";
      break;
    case AUTO_INCREMENT:
      result << ", AUTO_INCREMENT";
      break;
    case EVICTABLE:
      result << ", EVICTABLE";
      break;
    case LAST_VERSION:
      result << ", LAST_VERSION";
      break;
    case MAX_INDEX_ID:
      result << ", MAX_INDEX_ID";
      break;
    case HAS_KEY_PATH:
      result << ", HAS_KEY_PATH";
      break;
    case KEY_GENERATOR_CURRENT_NUMBER:
      result << ", KEY_GENERATOR_CURRENT_NUMBER";
      break;
    default:
      result << ", INVALID_TYPE";
  }
  result << "}";
  return result.str();
}

IndexMetaDataKey::IndexMetaDataKey()
    : object_store_id_(-1), index_id_(-1), meta_data_type_(0) {}

bool IndexMetaDataKey::Decode(StringPiece* slice, IndexMetaDataKey* result) {
  KeyPrefix prefix;
  if (!KeyPrefix::Decode(slice, &prefix))
    return false;
  DCHECK(prefix.database_id_);
  DCHECK(!prefix.object_store_id_);
  DCHECK(!prefix.index_id_);
  unsigned char type_byte = 0;
  if (!DecodeByte(slice, &type_byte))
    return false;
  DCHECK_EQ(type_byte, kIndexMetaDataTypeByte);
  if (!DecodeVarInt(slice, &result->object_store_id_))
    return false;
  if (!DecodeVarInt(slice, &result->index_id_))
    return false;
  if (!DecodeByte(slice, &result->meta_data_type_))
    return false;
  return true;
}

std::string IndexMetaDataKey::Encode(int64_t database_id,
                                     int64_t object_store_id,
                                     int64_t index_id,
                                     unsigned char meta_data_type) {
  KeyPrefix prefix(database_id);
  std::string ret = prefix.Encode();
  ret.push_back(kIndexMetaDataTypeByte);
  EncodeVarInt(object_store_id, &ret);
  EncodeVarInt(index_id, &ret);
  EncodeByte(meta_data_type, &ret);
  return ret;
}

std::string IndexMetaDataKey::EncodeMaxKey(int64_t database_id,
                                           int64_t object_store_id) {
  return Encode(database_id, object_store_id,
                std::numeric_limits<int64_t>::max(), kIndexMetaDataTypeMaximum);
}

std::string IndexMetaDataKey::EncodeMaxKey(int64_t database_id,
                                           int64_t object_store_id,
                                           int64_t index_id) {
  return Encode(database_id, object_store_id, index_id,
                kIndexMetaDataTypeMaximum);
}

int IndexMetaDataKey::Compare(const IndexMetaDataKey& other) {
  DCHECK_GE(object_store_id_, 0);
  DCHECK_GE(index_id_, 0);

  if (int x = CompareInts(object_store_id_, other.object_store_id_))
    return x;
  if (int x = CompareInts(index_id_, other.index_id_))
    return x;
  return meta_data_type_ - other.meta_data_type_;
}

std::string IndexMetaDataKey::DebugString() const {
  std::stringstream result;
  result << "IndexMetaDataKey{os: " << object_store_id_
         << ", idx: " << index_id_;
  switch (meta_data_type_) {
    case NAME:
      result << ", NAME";
      break;
    case UNIQUE:
      result << ", UNIQUE";
      break;
    case KEY_PATH:
      result << ", KEY_PATH";
      break;
    case MULTI_ENTRY:
      result << ", MULTI_ENTRY";
      break;
    default:
      result << ", INVALID_TYPE";
  }
  result << "}";
  return result.str();
}

int64_t IndexMetaDataKey::IndexId() const {
  DCHECK_GE(index_id_, 0);
  return index_id_;
}

ObjectStoreFreeListKey::ObjectStoreFreeListKey() : object_store_id_(-1) {}

bool ObjectStoreFreeListKey::Decode(StringPiece* slice,
                                    ObjectStoreFreeListKey* result) {
  KeyPrefix prefix;
  if (!KeyPrefix::Decode(slice, &prefix))
    return false;
  DCHECK(prefix.database_id_);
  DCHECK(!prefix.object_store_id_);
  DCHECK(!prefix.index_id_);
  unsigned char type_byte = 0;
  if (!DecodeByte(slice, &type_byte))
    return false;
  DCHECK_EQ(type_byte, kObjectStoreFreeListTypeByte);
  if (!DecodeVarInt(slice, &result->object_store_id_))
    return false;
  return true;
}

std::string ObjectStoreFreeListKey::Encode(int64_t database_id,
                                           int64_t object_store_id) {
  KeyPrefix prefix(database_id);
  std::string ret = prefix.Encode();
  ret.push_back(kObjectStoreFreeListTypeByte);
  EncodeVarInt(object_store_id, &ret);
  return ret;
}

std::string ObjectStoreFreeListKey::EncodeMaxKey(int64_t database_id) {
  return Encode(database_id, std::numeric_limits<int64_t>::max());
}

int64_t ObjectStoreFreeListKey::ObjectStoreId() const {
  DCHECK_GE(object_store_id_, 0);
  return object_store_id_;
}

int ObjectStoreFreeListKey::Compare(const ObjectStoreFreeListKey& other) {
  // TODO(jsbell): It may seem strange that we're not comparing database id's,
  // but that comparison will have been made earlier.
  // We should probably make this more clear, though...
  DCHECK_GE(object_store_id_, 0);
  return CompareInts(object_store_id_, other.object_store_id_);
}

std::string ObjectStoreFreeListKey::DebugString() const {
  std::stringstream result;
  result << "ObjectStoreFreeListKey{os: " << object_store_id_ << "}";
  return result.str();
}

IndexFreeListKey::IndexFreeListKey() : object_store_id_(-1), index_id_(-1) {}

bool IndexFreeListKey::Decode(StringPiece* slice, IndexFreeListKey* result) {
  KeyPrefix prefix;
  if (!KeyPrefix::Decode(slice, &prefix))
    return false;
  DCHECK(prefix.database_id_);
  DCHECK(!prefix.object_store_id_);
  DCHECK(!prefix.index_id_);
  unsigned char type_byte = 0;
  if (!DecodeByte(slice, &type_byte))
    return false;
  DCHECK_EQ(type_byte, kIndexFreeListTypeByte);
  if (!DecodeVarInt(slice, &result->object_store_id_))
    return false;
  if (!DecodeVarInt(slice, &result->index_id_))
    return false;
  return true;
}

std::string IndexFreeListKey::Encode(int64_t database_id,
                                     int64_t object_store_id,
                                     int64_t index_id) {
  KeyPrefix prefix(database_id);
  std::string ret = prefix.Encode();
  ret.push_back(kIndexFreeListTypeByte);
  EncodeVarInt(object_store_id, &ret);
  EncodeVarInt(index_id, &ret);
  return ret;
}

std::string IndexFreeListKey::EncodeMaxKey(int64_t database_id,
                                           int64_t object_store_id) {
  return Encode(database_id, object_store_id,
                std::numeric_limits<int64_t>::max());
}

int IndexFreeListKey::Compare(const IndexFreeListKey& other) {
  DCHECK_GE(object_store_id_, 0);
  DCHECK_GE(index_id_, 0);
  if (int x = CompareInts(object_store_id_, other.object_store_id_))
    return x;
  return CompareInts(index_id_, other.index_id_);
}

std::string IndexFreeListKey::DebugString() const {
  std::stringstream result;
  result << "IndexFreeListKey{os: " << object_store_id_
         << ", idx: " << index_id_ << "}";
  return result.str();
}

int64_t IndexFreeListKey::ObjectStoreId() const {
  DCHECK_GE(object_store_id_, 0);
  return object_store_id_;
}

int64_t IndexFreeListKey::IndexId() const {
  DCHECK_GE(index_id_, 0);
  return index_id_;
}

// TODO(jsbell): We never use this to look up object store ids,
// because a mapping is kept in the IndexedDBDatabase. Can the
// mapping become unreliable?  Can we remove this?
bool ObjectStoreNamesKey::Decode(StringPiece* slice,
                                 ObjectStoreNamesKey* result) {
  KeyPrefix prefix;
  if (!KeyPrefix::Decode(slice, &prefix))
    return false;
  DCHECK(prefix.database_id_);
  DCHECK(!prefix.object_store_id_);
  DCHECK(!prefix.index_id_);
  unsigned char type_byte = 0;
  if (!DecodeByte(slice, &type_byte))
    return false;
  DCHECK_EQ(type_byte, kObjectStoreNamesTypeByte);
  if (!DecodeStringWithLength(slice, &result->object_store_name_))
    return false;
  return true;
}

std::string ObjectStoreNamesKey::Encode(
    int64_t database_id,
    const std::u16string& object_store_name) {
  KeyPrefix prefix(database_id);
  std::string ret = prefix.Encode();
  ret.push_back(kObjectStoreNamesTypeByte);
  EncodeStringWithLength(object_store_name, &ret);
  return ret;
}

int ObjectStoreNamesKey::Compare(const ObjectStoreNamesKey& other) {
  return object_store_name_.compare(other.object_store_name_);
}

std::string ObjectStoreNamesKey::DebugString() const {
  std::stringstream result;
  result << "ObjectStoreNamesKey{object_store_name: " << object_store_name_
         << "}";
  return result.str();
}

IndexNamesKey::IndexNamesKey() : object_store_id_(-1) {}

// TODO(jsbell): We never use this to look up index ids, because a mapping
// is kept at a higher level.
bool IndexNamesKey::Decode(StringPiece* slice, IndexNamesKey* result) {
  KeyPrefix prefix;
  if (!KeyPrefix::Decode(slice, &prefix))
    return false;
  DCHECK(prefix.database_id_);
  DCHECK(!prefix.object_store_id_);
  DCHECK(!prefix.index_id_);
  unsigned char type_byte = 0;
  if (!DecodeByte(slice, &type_byte))
    return false;
  DCHECK_EQ(type_byte, kIndexNamesKeyTypeByte);
  if (!DecodeVarInt(slice, &result->object_store_id_))
    return false;
  if (!DecodeStringWithLength(slice, &result->index_name_))
    return false;
  return true;
}

std::string IndexNamesKey::Encode(int64_t database_id,
                                  int64_t object_store_id,
                                  const std::u16string& index_name) {
  KeyPrefix prefix(database_id);
  std::string ret = prefix.Encode();
  ret.push_back(kIndexNamesKeyTypeByte);
  EncodeVarInt(object_store_id, &ret);
  EncodeStringWithLength(index_name, &ret);
  return ret;
}

int IndexNamesKey::Compare(const IndexNamesKey& other) {
  DCHECK_GE(object_store_id_, 0);
  if (int x = CompareInts(object_store_id_, other.object_store_id_))
    return x;
  return index_name_.compare(other.index_name_);
}

std::string IndexNamesKey::DebugString() const {
  std::stringstream result;
  result << "IndexNamesKey{os: " << object_store_id_
         << ", index_name: " << index_name_ << "}";
  return result.str();
}

ObjectStoreDataKey::ObjectStoreDataKey() {}
ObjectStoreDataKey::~ObjectStoreDataKey() {}

bool ObjectStoreDataKey::Decode(StringPiece* slice,
                                ObjectStoreDataKey* result) {
  KeyPrefix prefix;
  if (!KeyPrefix::Decode(slice, &prefix))
    return false;
  DCHECK(prefix.database_id_);
  DCHECK(prefix.object_store_id_);
  DCHECK_EQ(prefix.index_id_, kSpecialIndexNumber);
  if (!ExtractEncodedIDBKey(slice, &result->encoded_user_key_))
    return false;
  return true;
}

std::string ObjectStoreDataKey::Encode(int64_t database_id,
                                       int64_t object_store_id,
                                       const std::string encoded_user_key) {
  KeyPrefix prefix(KeyPrefix::CreateWithSpecialIndex(
      database_id, object_store_id, kSpecialIndexNumber));
  std::string ret = prefix.Encode();
  ret.append(encoded_user_key);

  return ret;
}

std::string ObjectStoreDataKey::Encode(int64_t database_id,
                                       int64_t object_store_id,
                                       const IndexedDBKey& user_key) {
  std::string encoded_key;
  EncodeIDBKey(user_key, &encoded_key);
  return Encode(database_id, object_store_id, encoded_key);
}

std::string ObjectStoreDataKey::DebugString() const {
  std::unique_ptr<blink::IndexedDBKey> key = user_key();
  std::stringstream result;
  result << "ObjectStoreDataKey{user_key: "
         << (key ? key->DebugString() : "Invalid") << "}";
  return result.str();
}

std::unique_ptr<IndexedDBKey> ObjectStoreDataKey::user_key() const {
  std::unique_ptr<IndexedDBKey> key;
  StringPiece slice(encoded_user_key_);
  if (!DecodeIDBKey(&slice, &key)) {
    // TODO(jsbell): Return error.
  }
  return key;
}

const int64_t ObjectStoreDataKey::kSpecialIndexNumber = kObjectStoreDataIndexId;

ExistsEntryKey::ExistsEntryKey() {}
ExistsEntryKey::~ExistsEntryKey() {}

bool ExistsEntryKey::Decode(StringPiece* slice, ExistsEntryKey* result) {
  KeyPrefix prefix;
  if (!KeyPrefix::Decode(slice, &prefix))
    return false;
  DCHECK(prefix.database_id_);
  DCHECK(prefix.object_store_id_);
  DCHECK_EQ(prefix.index_id_, kSpecialIndexNumber);
  if (!ExtractEncodedIDBKey(slice, &result->encoded_user_key_))
    return false;
  return true;
}

std::string ExistsEntryKey::Encode(int64_t database_id,
                                   int64_t object_store_id,
                                   const std::string& encoded_key) {
  KeyPrefix prefix(KeyPrefix::CreateWithSpecialIndex(
      database_id, object_store_id, kSpecialIndexNumber));
  std::string ret = prefix.Encode();
  ret.append(encoded_key);
  return ret;
}

std::string ExistsEntryKey::Encode(int64_t database_id,
                                   int64_t object_store_id,
                                   const IndexedDBKey& user_key) {
  std::string encoded_key;
  EncodeIDBKey(user_key, &encoded_key);
  return Encode(database_id, object_store_id, encoded_key);
}

std::string ExistsEntryKey::DebugString() const {
  std::unique_ptr<blink::IndexedDBKey> key = user_key();
  std::stringstream result;
  result << "ExistsEntryKey{user_key: "
         << (key ? key->DebugString() : "Invalid") << "}";
  return result.str();
}

std::unique_ptr<IndexedDBKey> ExistsEntryKey::user_key() const {
  std::unique_ptr<IndexedDBKey> key;
  StringPiece slice(encoded_user_key_);
  if (!DecodeIDBKey(&slice, &key)) {
    // TODO(jsbell): Return error.
  }
  return key;
}

const int64_t ExistsEntryKey::kSpecialIndexNumber = kExistsEntryIndexId;

bool BlobEntryKey::Decode(StringPiece* slice, BlobEntryKey* result) {
  KeyPrefix prefix;
  if (!KeyPrefix::Decode(slice, &prefix))
    return false;
  DCHECK(prefix.database_id_);
  DCHECK(prefix.object_store_id_);
  DCHECK_EQ(prefix.index_id_, kSpecialIndexNumber);

  if (!ExtractEncodedIDBKey(slice, &result->encoded_user_key_))
    return false;
  result->database_id_ = prefix.database_id_;
  result->object_store_id_ = prefix.object_store_id_;

  return true;
}

bool BlobEntryKey::FromObjectStoreDataKey(StringPiece* slice,
                                          BlobEntryKey* result) {
  KeyPrefix prefix;
  if (!KeyPrefix::Decode(slice, &prefix))
    return false;
  DCHECK(prefix.database_id_);
  DCHECK(prefix.object_store_id_);
  DCHECK_EQ(prefix.index_id_, ObjectStoreDataKey::kSpecialIndexNumber);

  if (!ExtractEncodedIDBKey(slice, &result->encoded_user_key_))
    return false;
  result->database_id_ = prefix.database_id_;
  result->object_store_id_ = prefix.object_store_id_;
  return true;
}

std::string BlobEntryKey::ReencodeToObjectStoreDataKey(StringPiece* slice) {
  // TODO(ericu): We could be more efficient here, since the suffix is the same.
  BlobEntryKey key;
  if (!Decode(slice, &key))
    return std::string();

  return ObjectStoreDataKey::Encode(key.database_id_, key.object_store_id_,
                                    key.encoded_user_key_);
}

std::string BlobEntryKey::EncodeMinKeyForObjectStore(int64_t database_id,
                                                     int64_t object_store_id) {
  // Our implied encoded_user_key_ here is empty, the lowest possible key.
  return Encode(database_id, object_store_id, std::string());
}

std::string BlobEntryKey::EncodeStopKeyForObjectStore(int64_t database_id,
                                                      int64_t object_store_id) {
  DCHECK(KeyPrefix::ValidIds(database_id, object_store_id));
  KeyPrefix prefix(KeyPrefix::CreateWithSpecialIndex(
      database_id, object_store_id, kSpecialIndexNumber + 1));
  return prefix.Encode();
}

std::string BlobEntryKey::Encode() const {
  DCHECK(!encoded_user_key_.empty());
  return Encode(database_id_, object_store_id_, encoded_user_key_);
}

std::string BlobEntryKey::Encode(int64_t database_id,
                                 int64_t object_store_id,
                                 const IndexedDBKey& user_key) {
  std::string encoded_key;
  EncodeIDBKey(user_key, &encoded_key);
  return Encode(database_id, object_store_id, encoded_key);
}

std::string BlobEntryKey::Encode(int64_t database_id,
                                 int64_t object_store_id,
                                 const std::string& encoded_user_key) {
  DCHECK(KeyPrefix::ValidIds(database_id, object_store_id));
  KeyPrefix prefix(KeyPrefix::CreateWithSpecialIndex(
      database_id, object_store_id, kSpecialIndexNumber));
  return prefix.Encode() + encoded_user_key;
}

std::string BlobEntryKey::DebugString() const {
  std::stringstream result;
  result << "BlobEntryKey{db: " << database_id_ << "os: " << object_store_id_
         << ", user_key: ";
  std::unique_ptr<blink::IndexedDBKey> key;
  StringPiece slice(encoded_user_key_);
  if (!DecodeIDBKey(&slice, &key)) {
    result << "Invalid";
  } else {
    result << key->DebugString();
  }
  result << "}";
  return result.str();
}

const int64_t BlobEntryKey::kSpecialIndexNumber = kBlobEntryIndexId;

IndexDataKey::IndexDataKey()
    : database_id_(-1),
      object_store_id_(-1),
      index_id_(-1),
      sequence_number_(-1) {}

IndexDataKey::IndexDataKey(IndexDataKey&& other) = default;

IndexDataKey::~IndexDataKey() {}

bool IndexDataKey::Decode(StringPiece* slice, IndexDataKey* result) {
  KeyPrefix prefix;
  if (!KeyPrefix::Decode(slice, &prefix))
    return false;
  if (prefix.database_id_ <= 0)
    return false;
  if (prefix.object_store_id_ <= 0)
    return false;
  if (prefix.index_id_ < kMinimumIndexId)
    return false;
  result->database_id_ = prefix.database_id_;
  result->object_store_id_ = prefix.object_store_id_;
  result->index_id_ = prefix.index_id_;
  result->sequence_number_ = -1;
  result->encoded_primary_key_ = MinIDBKey();

  if (!ExtractEncodedIDBKey(slice, &result->encoded_user_key_))
    return false;

  // [optional] sequence number
  if (slice->empty())
    return true;
  if (!DecodeVarInt(slice, &result->sequence_number_))
    return false;

  // [optional] primary key
  if (slice->empty())
    return true;
  if (!ExtractEncodedIDBKey(slice, &result->encoded_primary_key_))
    return false;
  return true;
}

std::string IndexDataKey::Encode(int64_t database_id,
                                 int64_t object_store_id,
                                 int64_t index_id,
                                 const std::string& encoded_user_key,
                                 const std::string& encoded_primary_key,
                                 int64_t sequence_number) {
  KeyPrefix prefix(database_id, object_store_id, index_id);
  std::string ret = prefix.Encode();
  ret.append(encoded_user_key);
  EncodeVarInt(sequence_number, &ret);
  ret.append(encoded_primary_key);
  return ret;
}

std::string IndexDataKey::Encode(int64_t database_id,
                                 int64_t object_store_id,
                                 int64_t index_id,
                                 const IndexedDBKey& user_key) {
  std::string encoded_key;
  EncodeIDBKey(user_key, &encoded_key);
  return Encode(database_id, object_store_id, index_id, encoded_key,
                MinIDBKey(), 0);
}

std::string IndexDataKey::Encode(int64_t database_id,
                                 int64_t object_store_id,
                                 int64_t index_id,
                                 const IndexedDBKey& user_key,
                                 const IndexedDBKey& user_primary_key) {
  std::string encoded_key;
  EncodeIDBKey(user_key, &encoded_key);
  std::string encoded_primary_key;
  EncodeIDBKey(user_primary_key, &encoded_primary_key);
  return Encode(database_id, object_store_id, index_id, encoded_key,
                encoded_primary_key, 0);
}

std::string IndexDataKey::EncodeMinKey(int64_t database_id,
                                       int64_t object_store_id,
                                       int64_t index_id) {
  return Encode(database_id, object_store_id, index_id, MinIDBKey(),
                MinIDBKey(), 0);
}

std::string IndexDataKey::EncodeMaxKey(int64_t database_id,
                                       int64_t object_store_id,
                                       int64_t index_id) {
  return Encode(database_id, object_store_id, index_id, MaxIDBKey(),
                MaxIDBKey(), std::numeric_limits<int64_t>::max());
}

std::string IndexDataKey::Encode() const {
  return Encode(database_id_, object_store_id_, index_id_, encoded_user_key_,
                encoded_primary_key_, sequence_number_);
}

std::string IndexDataKey::DebugString() const {
  std::unique_ptr<blink::IndexedDBKey> user = user_key();
  std::unique_ptr<blink::IndexedDBKey> primary = primary_key();
  std::stringstream result;
  result << "IndexDataKey{db: " << database_id_ << ", os: " << object_store_id_
         << ", idx: " << index_id_ << ", sequence_number: " << sequence_number_
         << ", user_key: " << (user ? user->DebugString() : "Invalid")
         << ", primary_key: " << (primary ? primary->DebugString() : "Invalid")
         << "}";
  return result.str();
}

int64_t IndexDataKey::DatabaseId() const {
  DCHECK_GE(database_id_, 0);
  return database_id_;
}

int64_t IndexDataKey::ObjectStoreId() const {
  DCHECK_GE(object_store_id_, 0);
  return object_store_id_;
}

int64_t IndexDataKey::IndexId() const {
  DCHECK_GE(index_id_, 0);
  return index_id_;
}

std::unique_ptr<IndexedDBKey> IndexDataKey::user_key() const {
  std::unique_ptr<IndexedDBKey> key;
  StringPiece slice(encoded_user_key_);
  if (!DecodeIDBKey(&slice, &key)) {
    // TODO(jsbell): Return error.
  }
  return key;
}

std::unique_ptr<IndexedDBKey> IndexDataKey::primary_key() const {
  std::unique_ptr<IndexedDBKey> key;
  StringPiece slice(encoded_primary_key_);
  if (!DecodeIDBKey(&slice, &key)) {
    // TODO(jsbell): Return error.
  }
  return key;
}

}  // namespace content
