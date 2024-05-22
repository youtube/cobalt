// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_UUID_H_
#define BASE_UUID_H_

#include <stdint.h>

#include <iosfwd>
#include <string>

#include "base/base_export.h"
#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "base/strings/string_piece.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"

namespace content {
class FileSystemAccessManagerImpl;
}

namespace base {

// DEPRECATED(crbug.com/1195446): Use Uuid::GenerateRandomV4() instead.
BASE_EXPORT std::string GenerateUuid();

// DEPRECATED(crbug.com/1195446): Use Uuid::ParseCaseInsensitive() and
// Uuid::is_valid() instead.
BASE_EXPORT bool IsValidUuid(StringPiece input);

// DEPRECATED(crbug.com/1195446): Use Uuid::ParseLowercase() and
// Uuid::is_valid() instead.
BASE_EXPORT bool IsValidUuidOutputString(StringPiece input);

class BASE_EXPORT Uuid {
 public:
  // Length in bytes of the input required to format the input as a Uuid in the
  // form of version 4.
  static constexpr size_t kGuidV4InputLength = 16;

  // Generate a 128-bit random Uuid in the form of version 4. see RFC 4122,
  // section 4.4. The format of Uuid version 4 must be
  // xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx, where y is one of [8, 9, a, b]. The
  // hexadecimal values "a" through "f" are output as lower case characters.
  // A cryptographically secure random source will be used, but consider using
  // UnguessableToken for greater type-safety if Uuid format is unnecessary.
  static Uuid GenerateRandomV4();

  // Formats a sequence of 16 random bytes as a Uuid in the form of version 4.
  // `input` must:
  // - have been randomly generated (e.g. created from an UnguessableToken), and
  // - be of length 16 (this is checked at compile-time).
  // Despite taking 128 bits of randomness, certain bits will always be
  // masked over to adhere to the V4 Uuid format.
  // Useful in cases where an opaque identifier that is generated from stable
  // inputs needs to be formatted as a V4 Uuid. Currently only exposed to the
  // File System Access API to return a V4 Uuid for the getUniqueId() method.
  static Uuid FormatRandomDataAsV4(
      base::span<const uint8_t, kGuidV4InputLength> input,
      base::PassKey<content::FileSystemAccessManagerImpl> pass_key);
  static Uuid FormatRandomDataAsV4ForTesting(
      base::span<const uint8_t, kGuidV4InputLength> input);

  // Returns a valid Uuid if the input string conforms to the Uuid format, and
  // an invalid Uuid otherwise. Note that this does NOT check if the hexadecimal
  // values "a" through "f" are in lower case characters.
  static Uuid ParseCaseInsensitive(StringPiece input);
  static Uuid ParseCaseInsensitive(StringPiece16 input);

  // Similar to ParseCaseInsensitive(), but all hexadecimal values "a" through
  // "f" must be lower case characters.
  static Uuid ParseLowercase(StringPiece input);
  static Uuid ParseLowercase(StringPiece16 input);

  // Constructs an invalid Uuid.
  Uuid();

  Uuid(const Uuid& other);
  Uuid& operator=(const Uuid& other);
  Uuid(Uuid&& other);
  Uuid& operator=(Uuid&& other);

  bool is_valid() const { return !lowercase_.empty(); }

  // Returns the Uuid in a lowercase string format if it is valid, and an empty
  // string otherwise. The returned value is guaranteed to be parsed by
  // ParseLowercase().
  //
  // NOTE: While AsLowercaseString() is currently a trivial getter, callers
  // should not treat it as such. When the internal type of base::Uuid changes,
  // this will be a non-trivial converter. See the TODO above `lowercase_` for
  // more context.
  const std::string& AsLowercaseString() const;

  // Invalid Uuids are equal.
  bool operator==(const Uuid& other) const;
  bool operator!=(const Uuid& other) const;
  bool operator<(const Uuid& other) const;
  bool operator<=(const Uuid& other) const;
  bool operator>(const Uuid& other) const;
  bool operator>=(const Uuid& other) const;

 private:
  static Uuid FormatRandomDataAsV4Impl(
      base::span<const uint8_t, kGuidV4InputLength> input);

  // TODO(crbug.com/1026195): Consider using a different internal type.
  // Most existing representations of Uuids in the codebase use std::string,
  // so matching the internal type will avoid inefficient string conversions
  // during the migration to base::Uuid.
  //
  // The lowercase form of the Uuid. Empty for invalid Uuids.
  std::string lowercase_;
};

// For runtime usage only. Do not store the result of this hash, as it may
// change in future Chromium revisions.
struct BASE_EXPORT UuidHash {
  size_t operator()(const Uuid& uuid) const {
    // TODO(crbug.com/1026195): Avoid converting to string to take the hash when
    // the internal type is migrated to a non-string type.
    return FastHash(uuid.AsLowercaseString());
  }
};

// Stream operator so Uuid objects can be used in logging statements.
BASE_EXPORT std::ostream& operator<<(std::ostream& out, const Uuid& uuid);

// DEPREACATED(crbug.com/1428566): Please, use the Uuid variants of the
// functions/types above. These are merely aliases to allow a gradual
// transition away from `base/guid.h`.
using GUID = Uuid;
using GUIDHash = UuidHash;
BASE_EXPORT std::string GenerateGUID();
BASE_EXPORT bool IsValidGUID(StringPiece input);
BASE_EXPORT bool IsValidGUIDOutputString(StringPiece input);

}  // namespace base

#endif  // BASE_UUID_H_
