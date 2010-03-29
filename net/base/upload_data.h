// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_UPLOAD_DATA_H_
#define NET_BASE_UPLOAD_DATA_H_

#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "net/base/file_stream.h"
#include "base/time.h"
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
                file_range_length_(kuint64max),
                override_content_length_(false),
                content_length_computed_(false),
                file_stream_(NULL) {
    }

    ~Element() {
      // In the common case |file__stream_| will be null.
      delete file_stream_;
    }

    Type type() const { return type_; }
    const std::vector<char>& bytes() const { return bytes_; }
    const FilePath& file_path() const { return file_path_; }
    uint64 file_range_offset() const { return file_range_offset_; }
    uint64 file_range_length() const { return file_range_length_; }
    // If NULL time is returned, we do not do the check.
    const base::Time& expected_file_modification_time() const {
      return expected_file_modification_time_;
    }

    void SetToBytes(const char* bytes, int bytes_len) {
      type_ = TYPE_BYTES;
      bytes_.assign(bytes, bytes + bytes_len);
    }

    void SetToFilePath(const FilePath& path) {
      SetToFilePathRange(path, 0, kuint64max, base::Time());
    }

    // If expected_modification_time is NULL, we do not check for the file
    // change. Also note that the granularity for comparison is time_t, not
    // the full precision.
    void SetToFilePathRange(const FilePath& path,
                            uint64 offset, uint64 length,
                            const base::Time& expected_modification_time) {
      type_ = TYPE_FILE;
      file_path_ = path;
      file_range_offset_ = offset;
      file_range_length_ = length;
      expected_file_modification_time_ = expected_modification_time;
    }

    // Returns the byte-length of the element.  For files that do not exist, 0
    // is returned.  This is done for consistency with Mozilla.
    // Once called, this function will always return the same value.
    uint64 GetContentLength();

    // Returns a FileStream opened for reading for this element, positioned at
    // |file_range_offset_|.  The caller gets ownership and is responsible
    // for cleaning up the FileStream. Returns NULL if this element is not of
    // type TYPE_FILE or if the file is not openable.
    FileStream* NewFileStreamForReading();

   private:
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
    base::Time expected_file_modification_time_;
    bool override_content_length_;
    bool content_length_computed_;
    uint64 content_length_;
    FileStream* file_stream_;

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
                       uint64 offset, uint64 length,
                       const base::Time& expected_modification_time) {
    elements_.push_back(Element());
    elements_.back().SetToFilePathRange(file_path, offset, length,
                                        expected_modification_time);
  }

  // Returns the total size in bytes of the data to upload.
  uint64 GetContentLength();

  std::vector<Element>* elements() {
    return &elements_;
  }

  void set_elements(const std::vector<Element>& elements) {
    elements_ = elements;
  }

  void swap_elements(std::vector<Element>* elements) {
    elements_.swap(*elements);
  }

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
