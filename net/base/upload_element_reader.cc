// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_element_reader.h"

#include "base/logging.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_element.h"
#include "net/base/upload_file_element_reader.h"

namespace net {

// static
UploadElementReader* UploadElementReader::Create(const UploadElement& element) {
  UploadElementReader* reader = NULL;
  switch (element.type()) {
    case UploadElement::TYPE_BYTES:
      reader = new UploadBytesElementReader(element.bytes(),
                                            element.bytes_length());
      break;
    case UploadElement::TYPE_FILE:
      reader = new UploadFileElementReader(
          element.file_path(),
          element.file_range_offset(),
          element.file_range_length(),
          element.expected_file_modification_time());
      break;
  }
  DCHECK(reader);
  return reader;
}

bool UploadElementReader::IsInMemory() const {
  return false;
}

}  // namespace net
