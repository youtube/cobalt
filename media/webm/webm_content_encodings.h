// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBM_WEBM_CONTENT_ENCODINGS_H_
#define MEDIA_WEBM_WEBM_CONTENT_ENCODINGS_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"

namespace media {

struct MEDIA_EXPORT ContentEncoding : public base::RefCounted<ContentEncoding> {
  // The following enum definitions are based on the ContentEncoding element
  // specified in the Matroska spec.

  static const int kOrderInvalid = -1;

  enum Scope {
    kScopeInvalid = 0,
    kScopeAllFrameContents = 1,
    kScopeTrackPrivateData = 2,
    kScopeNextContentEncodingData = 4,
    kScopeMax = 7,
  };

  enum Type {
    kTypeInvalid = -1,
    kTypeCompression = 0,
    kTypeEncryption = 1,
  };

  enum EncryptionAlgo {
    kEncAlgoInvalid = -1,
    kEncAlgoNotEncrypted = 0,
    kEncAlgoDes = 1,
    kEncAlgo3des = 2,
    kEncAlgoTwofish = 3,
    kEncAlgoBlowfish = 4,
    kEncAlgoAes = 5,
  };

  ContentEncoding();
  virtual ~ContentEncoding();

  int64 order_;
  Scope scope_;
  Type type_;
  EncryptionAlgo encryption_algo_;
  scoped_array<uint8> encryption_key_id_;
  int encryption_key_id_size_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentEncoding);
};

typedef std::vector<scoped_refptr<ContentEncoding> > ContentEncodings;

}  // namespace media

#endif  // MEDIA_WEBM_WEBM_CONTENT_ENCODINGS_H_
