// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/file_system_url.h"

#include <sstream>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "storage/browser/file_system/file_system_features.h"
#include "storage/browser/file_system/file_system_util.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace storage {

namespace {

bool AreSameStorageKey(const FileSystemURL& a, const FileSystemURL& b) {
  // TODO(https://crbug.com/1396116): Make the `storage_key_` member optional.
  // This class improperly uses a StorageKey with an opaque origin to indicate a
  // lack of origin for FileSystemURLs corresponding to non-sandboxed file
  // systems. This leads to unexpected behavior when comparing two non-sandboxed
  // FileSystemURLs which differ only in the nonce of their default-constructed
  // StorageKey.
  return base::FeatureList::IsEnabled(
             features::kFileSystemURLComparatorsTreatOpaqueOriginAsNoOrigin)
             ? a.storage_key() == b.storage_key() ||
                   (a.type() == b.type() &&
                    (a.type() == storage::kFileSystemTypeExternal ||
                     a.type() == storage::kFileSystemTypeLocal) &&
                    a.storage_key().origin().opaque() &&
                    b.storage_key().origin().opaque())
             : a.storage_key() == b.storage_key();
}

}  // namespace

FileSystemURL::FileSystemURL()
    : is_null_(true),
      is_valid_(false),
      mount_type_(kFileSystemTypeUnknown),
      type_(kFileSystemTypeUnknown),
      mount_option_(FlushPolicy::NO_FLUSH_ON_COMPLETION) {}

FileSystemURL::FileSystemURL(const FileSystemURL&) = default;

FileSystemURL::FileSystemURL(FileSystemURL&&) noexcept = default;

FileSystemURL& FileSystemURL::operator=(const FileSystemURL&) = default;

FileSystemURL& FileSystemURL::operator=(FileSystemURL&&) noexcept = default;

FileSystemURL::~FileSystemURL() = default;

// static
FileSystemURL FileSystemURL::CreateForTest(const GURL& url) {
  return FileSystemURL(
      url, blink::StorageKey::CreateFirstParty(url::Origin::Create(url)));
}

FileSystemURL FileSystemURL::CreateForTest(const blink::StorageKey& storage_key,
                                           FileSystemType mount_type,
                                           const base::FilePath& virtual_path) {
  return FileSystemURL(storage_key, mount_type, virtual_path);
}

FileSystemURL FileSystemURL::CreateForTest(
    const blink::StorageKey& storage_key,
    FileSystemType mount_type,
    const base::FilePath& virtual_path,
    const std::string& mount_filesystem_id,
    FileSystemType cracked_type,
    const base::FilePath& cracked_path,
    const std::string& filesystem_id,
    const FileSystemMountOption& mount_option) {
  return FileSystemURL(storage_key, mount_type, virtual_path,
                       mount_filesystem_id, cracked_type, cracked_path,
                       filesystem_id, mount_option);
}

FileSystemURL::FileSystemURL(const GURL& url,
                             const blink::StorageKey& storage_key)
    : is_null_(false),
      mount_type_(kFileSystemTypeUnknown),
      type_(kFileSystemTypeUnknown),
      mount_option_(FlushPolicy::NO_FLUSH_ON_COMPLETION) {
  GURL origin_url;
  // URL should be able to be parsed and the parsed origin should match the
  // StorageKey's origin member.
  is_valid_ = ParseFileSystemSchemeURL(url, &origin_url, &mount_type_,
                                       &virtual_path_) &&
              storage_key.origin().IsSameOriginWith(origin_url);
  storage_key_ = storage_key;
  path_ = virtual_path_;
  type_ = mount_type_;
}

FileSystemURL::FileSystemURL(const blink::StorageKey& storage_key,
                             FileSystemType mount_type,
                             const base::FilePath& virtual_path)
    : is_null_(false),
      is_valid_(true),
      storage_key_(storage_key),
      mount_type_(mount_type),
      virtual_path_(virtual_path.NormalizePathSeparators()),
      type_(mount_type),
      path_(virtual_path.NormalizePathSeparators()),
      mount_option_(FlushPolicy::NO_FLUSH_ON_COMPLETION) {}

FileSystemURL::FileSystemURL(const blink::StorageKey& storage_key,
                             FileSystemType mount_type,
                             const base::FilePath& virtual_path,
                             const std::string& mount_filesystem_id,
                             FileSystemType cracked_type,
                             const base::FilePath& cracked_path,
                             const std::string& filesystem_id,
                             const FileSystemMountOption& mount_option)
    : is_null_(false),
      is_valid_(true),
      storage_key_(storage_key),
      mount_type_(mount_type),
      virtual_path_(virtual_path.NormalizePathSeparators()),
      mount_filesystem_id_(mount_filesystem_id),
      type_(cracked_type),
      path_(cracked_path.NormalizePathSeparators()),
      filesystem_id_(filesystem_id),
      mount_option_(mount_option) {}

GURL FileSystemURL::ToGURL() const {
  if (!is_valid_)
    return GURL();

  GURL url = GetFileSystemRootURI(storage_key_.origin().GetURL(), mount_type_);
  if (!url.is_valid())
    return GURL();

  std::string url_string = url.spec();

  // Exactly match with DOMFileSystemBase::createFileSystemURL()'s encoding
  // behavior, where the path is escaped by KURL::encodeWithURLEscapeSequences
  // which is essentially encodeURIComponent except '/'.
  std::string escaped = base::EscapeQueryParamValue(
      virtual_path_.NormalizePathSeparatorsTo('/').AsUTF8Unsafe(),
      false /* use_plus */);
  base::ReplaceSubstringsAfterOffset(&escaped, 0, "%2F", "/");
  url_string.append(escaped);

  // Build nested GURL.
  return GURL(url_string);
}

std::string FileSystemURL::DebugString() const {
  if (!is_valid_)
    return "invalid filesystem: URL";
  std::ostringstream ss;
  switch (mount_type_) {
    // Include GURL if GURL serialization is possible.
    case kFileSystemTypeTemporary:
    case kFileSystemTypePersistent:
    case kFileSystemTypeExternal:
    case kFileSystemTypeIsolated:
    case kFileSystemTypeTest:
      ss << "{ uri: ";
      ss << GetFileSystemRootURI(storage_key_.origin().GetURL(), mount_type_);
      break;
    // Otherwise list the origin and path separately.
    default:
      ss << "{ path: ";
  }

  // filesystem_id_ will be non empty for (and only for) cracked URLs.
  if (!filesystem_id_.empty()) {
    ss << virtual_path_.value();
    ss << " (";
    ss << GetFileSystemTypeString(type_) << "@" << filesystem_id_ << ":";
    ss << path_.value();
    ss << ")";
  } else {
    ss << path_.value();
  }
  ss << ", storage key: " << storage_key_.GetDebugString();
  if (bucket_.has_value()) {
    ss << ", bucket id: " << bucket_->id;
  }
  ss << " }";
  return ss.str();
}

BucketLocator FileSystemURL::GetBucket() const {
  if (bucket())
    return *bucket_;

  auto bucket = storage::BucketLocator::ForDefaultBucket(storage_key());
  bucket.type = storage::FileSystemTypeToQuotaStorageType(type());
  return bucket;
}

bool FileSystemURL::IsParent(const FileSystemURL& child) const {
  return IsInSameFileSystem(child) && path().IsParent(child.path());
}

bool FileSystemURL::IsInSameFileSystem(const FileSystemURL& other) const {
  // Invalid FileSystemURLs should never be considered of the same file system.
  bool is_maybe_valid =
      !base::FeatureList::IsEnabled(
          features::kFileSystemURLComparatorsTreatOpaqueOriginAsNoOrigin) ||
      (is_valid() && other.is_valid());
  return AreSameStorageKey(*this, other) && is_maybe_valid &&
         type() == other.type() && filesystem_id() == other.filesystem_id() &&
         bucket() == other.bucket();
}

bool FileSystemURL::operator==(const FileSystemURL& that) const {
  if (is_null_ && that.is_null_) {
    return true;
  }

  return AreSameStorageKey(*this, that) && type_ == that.type_ &&
         path_ == that.path_ && filesystem_id_ == that.filesystem_id_ &&
         is_valid_ == that.is_valid_ && bucket_ == that.bucket_;
}

bool FileSystemURL::Comparator::operator()(const FileSystemURL& lhs,
                                           const FileSystemURL& rhs) const {
  DCHECK(lhs.is_valid_ && rhs.is_valid_);
  if (!AreSameStorageKey(lhs, rhs)) {
    return lhs.storage_key() < rhs.storage_key();
  }
  if (lhs.type_ != rhs.type_) {
    return lhs.type_ < rhs.type_;
  }
  if (lhs.filesystem_id_ != rhs.filesystem_id_) {
    return lhs.filesystem_id_ < rhs.filesystem_id_;
  }
  if (lhs.bucket_ != rhs.bucket_) {
    return lhs.bucket_ < rhs.bucket_;
  }
  return lhs.path_ < rhs.path_;
}

}  // namespace storage
