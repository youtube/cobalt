// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_UPLOAD_BYTES_ELEMENT_READER_H_
#define NET_BASE_UPLOAD_BYTES_ELEMENT_READER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/base/upload_element_reader.h"

namespace net {

// An UploadElementReader implementation for bytes.
// |data| should outlive this class because this class does not take the
// ownership of the data.
class NET_EXPORT UploadBytesElementReader : public UploadElementReader {
 public:
  UploadBytesElementReader(const char* bytes, uint64 length);
  virtual ~UploadBytesElementReader();

  const char* bytes() const { return bytes_; }
  uint64 length() const { return length_; }

  // UploadElementReader overrides:
  virtual const UploadBytesElementReader* AsBytesReader() const override;
  virtual int Init(const CompletionCallback& callback) override;
  virtual int InitSync() override;
  virtual uint64 GetContentLength() const override;
  virtual uint64 BytesRemaining() const override;
  virtual bool IsInMemory() const override;
  virtual int Read(IOBuffer* buf,
                   int buf_length,
                   const CompletionCallback& callback) override;
  virtual int ReadSync(IOBuffer* buf, int buf_length) override;

 private:
  const char* const bytes_;
  const uint64 length_;
  uint64 offset_;

  DISALLOW_COPY_AND_ASSIGN(UploadBytesElementReader);
};

// A subclass of UplodBytesElementReader which owns the data given as a vector.
class NET_EXPORT UploadOwnedBytesElementReader
    : public UploadBytesElementReader {
 public:
  // |data| is cleared by this ctor.
  explicit UploadOwnedBytesElementReader(std::vector<char>* data);
  virtual ~UploadOwnedBytesElementReader();

  // Creates UploadOwnedBytesElementReader with a string.
  static UploadOwnedBytesElementReader* CreateWithString(
      const std::string& string);

 private:
  std::vector<char> data_;

  DISALLOW_COPY_AND_ASSIGN(UploadOwnedBytesElementReader);
};

}  // namespace net

#endif  // NET_BASE_UPLOAD_BYTES_ELEMENT_READER_H_
