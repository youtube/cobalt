// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/archive_validator.h"

#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "build/build_config.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/content_uri_utils.h"
#endif

namespace offline_pages {

ArchiveValidator::ArchiveValidator() {
  secure_hash_ = crypto::SecureHash::Create(crypto::SecureHash::SHA256);
}

ArchiveValidator::~ArchiveValidator() = default;

void ArchiveValidator::Update(const char* input, size_t len) {
  secure_hash_->Update(input, len);
}

std::string ArchiveValidator::Finish() {
  std::string digest(crypto::kSHA256Length, 0);
  secure_hash_->Finish(&(digest[0]), digest.size());
  return digest;
}

// static
std::string ArchiveValidator::ComputeDigest(const base::FilePath& file_path) {
  std::pair<int64_t, std::string> result = GetSizeAndComputeDigest(file_path);
  return result.second;
}

// static
std::pair<int64_t, std::string> ArchiveValidator::GetSizeAndComputeDigest(
    const base::FilePath& file_path) {
  base::File file;
#if BUILDFLAG(IS_ANDROID)
  if (file_path.IsContentUri()) {
    file = base::OpenContentUriForRead(file_path);
  } else {
#endif  // BUILDFLAG(IS_ANDROID)
    file.Initialize(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
#if BUILDFLAG(IS_ANDROID)
  }
#endif  // BUILDFLAG(IS_ANDROID)
  if (!file.IsValid())
    return std::make_pair(0LL, std::string());

  ArchiveValidator archive_validator;

  const int kMaxBufferSize = 1024;
  std::vector<char> buffer(kMaxBufferSize);
  int64_t total_read = 0LL;
  int bytes_read;
  do {
    bytes_read = file.ReadAtCurrentPos(buffer.data(), kMaxBufferSize);
    if (bytes_read > 0) {
      total_read += bytes_read;
      archive_validator.Update(buffer.data(), bytes_read);
    }
  } while (bytes_read > 0);
  if (bytes_read < 0)
    return std::make_pair(0LL, std::string());

  return std::make_pair(total_read, archive_validator.Finish());
}

// static
bool ArchiveValidator::ValidateFile(const base::FilePath& file_path,
                                    int64_t expected_file_size,
                                    const std::string& expected_digest) {
  int64_t actual_file_size;
  if (!base::GetFileSize(file_path, &actual_file_size))
    return false;
  if (expected_file_size != actual_file_size)
    return false;

  std::string actual_digest = ComputeDigest(file_path);
  return expected_digest == actual_digest;
}

}  // namespace offline_pages
