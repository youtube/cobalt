// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_element_reader.h"

#include "base/logging.h"
#include "net/base/net_errors.h"
#include "net/base/upload_element.h"

namespace net {

const UploadBytesElementReader* UploadElementReader::AsBytesReader() const {
  return NULL;
}

const UploadFileElementReader* UploadElementReader::AsFileReader() const {
  return NULL;
}

int UploadElementReader::InitSync() {
  NOTREACHED() << "This instance does not support InitSync().";
  return ERR_NOT_IMPLEMENTED;
}

bool UploadElementReader::IsInMemory() const {
  return false;
}

int UploadElementReader::ReadSync(IOBuffer* buf, int buf_length) {
  NOTREACHED() << "This instance does not support ReadSync().";
  return ERR_NOT_IMPLEMENTED;
}

}  // namespace net
