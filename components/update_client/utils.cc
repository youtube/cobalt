// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/utils.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/json/json_file_value_serializer.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/crx_file/id_util.h"
#include "components/update_client/component.h"
#include "components/update_client/configurator.h"
#include "components/update_client/network.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "url/gurl.h"

namespace update_client {

bool HasDiffUpdate(const Component& component) {
  return !component.crx_diffurls().empty();
}

bool IsHttpServerError(int status_code) {
  return 500 <= status_code && status_code < 600;
}

bool DeleteFileAndEmptyParentDirectory(const base::FilePath& filepath) {
  if (!base::DeleteFile(filepath, false))
    return false;

  const base::FilePath dirname(filepath.DirName());
  if (!base::IsDirectoryEmpty(dirname))
    return true;

  return base::DeleteFile(dirname, false);
}

std::string GetCrxComponentID(const CrxComponent& component) {
  return component.app_id.empty() ? GetCrxIdFromPublicKeyHash(component.pk_hash)
                                  : component.app_id;
}

std::string GetCrxIdFromPublicKeyHash(const std::vector<uint8_t>& pk_hash) {
  const std::string result =
      crx_file::id_util::GenerateIdFromHash(&pk_hash[0], pk_hash.size());
  DCHECK(crx_file::id_util::IdIsValid(result));
  return result;
}

#if defined(STARBOARD)
#if defined(IN_MEMORY_UPDATES)
bool VerifyHash256(const std::string* content,
                   const std::string& expected_hash_str) {
  std::vector<uint8_t> expected_hash;
  if (!base::HexStringToBytes(expected_hash_str, &expected_hash) ||
      expected_hash.size() != crypto::kSHA256Length) {
    return false;
  }

  uint8_t actual_hash[crypto::kSHA256Length] = {0};
  std::unique_ptr<crypto::SecureHash> hasher(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));

  hasher->Update(content->c_str(), content->size());
  hasher->Finish(actual_hash, sizeof(actual_hash));

  return memcmp(actual_hash, &expected_hash[0], sizeof(actual_hash)) == 0;
}
#else  // defined(IN_MEMORY_UPDATES)
bool VerifyFileHash256(const base::FilePath& filepath,
                       const std::string& expected_hash_str) {
  std::vector<uint8_t> expected_hash;
  if (!base::HexStringToBytes(expected_hash_str, &expected_hash) ||
      expected_hash.size() != crypto::kSHA256Length) {
    return false;
  }

  base::File source_file(filepath,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!source_file.IsValid()) {
    DPLOG(ERROR) << "VerifyFileHash256(): Unable to open source file: "
                 << filepath.value();
    return false;
  }

  const size_t kBufferSize = 32768;
  std::vector<char> buffer(kBufferSize);
  uint8_t actual_hash[crypto::kSHA256Length] = {0};
  std::unique_ptr<crypto::SecureHash> hasher(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));

  while (true) {
    int bytes_read = source_file.ReadAtCurrentPos(&buffer[0], buffer.size());
    if (bytes_read < 0) {
      DPLOG(ERROR) << "VerifyFileHash256(): error reading from source file: "
                   << filepath.value();

      return false;
    }

    if (bytes_read == 0) {
      break;
    }

    hasher->Update(&buffer[0], bytes_read);
  }

  hasher->Finish(actual_hash, sizeof(actual_hash));

  return memcmp(actual_hash, &expected_hash[0], sizeof(actual_hash)) == 0;
}
#endif  // defined(IN_MEMORY_UPDATES)
#else  // defined(STARBOARD)
bool VerifyFileHash256(const base::FilePath& filepath,
                       const std::string& expected_hash_str) {
  std::vector<uint8_t> expected_hash;
  if (!base::HexStringToBytes(expected_hash_str, &expected_hash) ||
      expected_hash.size() != crypto::kSHA256Length) {
    return false;
  }
  base::MemoryMappedFile mmfile;
  if (!mmfile.Initialize(filepath))
    return false;

  uint8_t actual_hash[crypto::kSHA256Length] = {0};
  std::unique_ptr<crypto::SecureHash> hasher(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));
  hasher->Update(mmfile.data(), mmfile.length());
  hasher->Finish(actual_hash, sizeof(actual_hash));

  return memcmp(actual_hash, &expected_hash[0], sizeof(actual_hash)) == 0;
}
#endif

bool IsValidBrand(const std::string& brand) {
  const size_t kMaxBrandSize = 4;
  if (!brand.empty() && brand.size() != kMaxBrandSize)
    return false;

  return std::find_if_not(brand.begin(), brand.end(), [](char ch) {
           return base::IsAsciiAlpha(ch);
         }) == brand.end();
}

// Helper function.
// Returns true if |part| matches the expression
// ^[<special_chars>a-zA-Z0-9]{min_length,max_length}$
bool IsValidInstallerAttributePart(const std::string& part,
                                   const std::string& special_chars,
                                   size_t min_length,
                                   size_t max_length) {
  if (part.size() < min_length || part.size() > max_length)
    return false;

  return std::find_if_not(part.begin(), part.end(), [&special_chars](char ch) {
           if (base::IsAsciiAlpha(ch) || base::IsAsciiDigit(ch))
             return true;

           for (auto c : special_chars) {
             if (c == ch)
               return true;
           }

           return false;
         }) == part.end();
}

// Returns true if the |name| parameter matches ^[-_a-zA-Z0-9]{1,256}$ .
bool IsValidInstallerAttributeName(const std::string& name) {
  return IsValidInstallerAttributePart(name, "-_", 1, 256);
}

// Returns true if the |value| parameter matches ^[-.,;+_=a-zA-Z0-9]{0,256}$ .
bool IsValidInstallerAttributeValue(const std::string& value) {
  return IsValidInstallerAttributePart(value, "-.,;+_=", 0, 256);
}

bool IsValidInstallerAttribute(const InstallerAttribute& attr) {
  return IsValidInstallerAttributeName(attr.first) &&
         IsValidInstallerAttributeValue(attr.second);
}

void RemoveUnsecureUrls(std::vector<GURL>* urls) {
  DCHECK(urls);
  base::EraseIf(*urls,
                [](const GURL& url) { return !url.SchemeIsCryptographic(); });
}

CrxInstaller::Result InstallFunctionWrapper(
    base::OnceCallback<bool()> callback) {
  return CrxInstaller::Result(std::move(callback).Run()
                                  ? InstallError::NONE
                                  : InstallError::GENERIC_ERROR);
}

// TODO(cpu): add a specific attribute check to a component json that the
// extension unpacker will reject, so that a component cannot be installed
// as an extension.
std::unique_ptr<base::DictionaryValue> ReadManifest(
    const base::FilePath& unpack_path) {
  base::FilePath manifest =
      unpack_path.Append(FILE_PATH_LITERAL("manifest.json"));
  if (!base::PathExists(manifest))
    return std::unique_ptr<base::DictionaryValue>();
  JSONFileValueDeserializer deserializer(manifest);
  std::string error;
  std::unique_ptr<base::Value> root = deserializer.Deserialize(nullptr, &error);
  if (!root)
    return std::unique_ptr<base::DictionaryValue>();
  if (!root->is_dict())
    return std::unique_ptr<base::DictionaryValue>();
  return std::unique_ptr<base::DictionaryValue>(
      static_cast<base::DictionaryValue*>(root.release()));
}

}  // namespace update_client
