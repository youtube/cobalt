// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_UPLOAD_DATA_H_
#define NET_BASE_UPLOAD_DATA_H_

#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/platform_file.h"
#include "base/ref_counted.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace net {

class UploadData : public base::RefCounted<UploadData> {
 public:
  UploadData() : identifier_(0) {}

  enum Type {
    TYPE_BYTES,
    TYPE_FILE
  };

  class Element {
   public:
    Element() : type_(TYPE_BYTES), file_range_offset_(0),
                file_range_length_(0),
                file_(base::kInvalidPlatformFileValue),
                override_content_length_(false) {
    }

    Type type() const { return type_; }
    const std::vector<char>& bytes() const { return bytes_; }
    const FilePath& file_path() const { return file_path_; }
    uint64 file_range_offset() const { return file_range_offset_; }
    uint64 file_range_length() const { return file_range_length_; }

    void SetToBytes(const char* bytes, int bytes_len) {
      type_ = TYPE_BYTES;
      bytes_.assign(bytes, bytes + bytes_len);
    }

    void SetToFilePath(const FilePath& path) {
      SetToFilePathRange(path, 0, kuint64max);
    }

    void SetToFilePathRange(const FilePath& path, uint64 offset, uint64 length);

    // Returns the byte-length of the element.  For files that do not exist, 0
    // is returned.  This is done for consistency with Mozilla.
    uint64 GetContentLength() const {
      if (override_content_length_)
        return content_length_;

      if (type_ == TYPE_BYTES) {
        return bytes_.size();
      } else {
        return file_range_length_;
      }
    }

    // For a TYPE_FILE, return a handle to the file. The caller does not take
    // ownership and should not close the file handle.
    base::PlatformFile platform_file() const;

    // For a TYPE_FILE, this closes the file handle. It's a fatal error to call
    // platform_file() after this.
    void Close();

   private:
    // type_ == TYPE_BYTES:
    //   bytes_ is valid
    // type_ == TYPE_FILE:
    //   file_path_ should always be valid.
    //
    //   platform_file() may be invalid, in which case file_range_* are 0 and
    //   file_ is invalid. This occurs when we cannot open the requested file.
    //
    //   Else, then file_range_* are within range of the length of the file
    //   that we found when opening the file. Also, the sum of offset and
    //   length will not overflow a uint64. file_ will be handle to the file.

    // Allows tests to override the result of GetContentLength.
    void SetContentLength(uint64 content_length) {
      override_content_length_ = true;
      content_length_ = content_length;
    }

    Type type_;
    std::vector<char> bytes_;
    FilePath file_path_;
    uint64 file_range_offset_;
    uint64 file_range_length_;
    base::PlatformFile file_;
    bool override_content_length_;
    uint64 content_length_;

    FRIEND_TEST(UploadDataStreamTest, FileSmallerThanLength);
    FRIEND_TEST(HttpNetworkTransactionTest, UploadFileSmallerThanLength);
  };

  void AppendBytes(const char* bytes, int bytes_len) {
    if (bytes_len > 0) {
      elements_.push_back(Element());
      elements_.back().SetToBytes(bytes, bytes_len);
    }
  }

  void AppendFile(const FilePath& file_path) {
    elements_.push_back(Element());
    elements_.back().SetToFilePath(file_path);
  }

  void AppendFileRange(const FilePath& file_path,
                       uint64 offset, uint64 length) {
    elements_.push_back(Element());
    elements_.back().SetToFilePathRange(file_path, offset, length);
  }

  // Returns the total size in bytes of the data to upload.
  uint64 GetContentLength() const;

  const std::vector<Element>& elements() const {
    return elements_;
  }

  void set_elements(const std::vector<Element>& elements) {
    elements_ = elements;
  }

  void swap_elements(std::vector<Element>* elements) {
    elements_.swap(*elements);
  }

  // CloseFiles closes the file handles of all Elements of type TYPE_FILE.
  void CloseFiles();

  // Identifies a particular upload instance, which is used by the cache to
  // formulate a cache key.  This value should be unique across browser
  // sessions.  A value of 0 is used to indicate an unspecified identifier.
  void set_identifier(int64 id) { identifier_ = id; }
  int64 identifier() const { return identifier_; }

 private:
  friend class base::RefCounted<UploadData>;

  ~UploadData() {}

  std::vector<Element> elements_;
  int64 identifier_;
};

}  // namespace net

#endif  // NET_BASE_UPLOAD_DATA_H_
