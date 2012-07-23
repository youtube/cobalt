// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mp4/cenc.h"

#include "media/mp4/box_reader.h"
#include "media/mp4/rcheck.h"

namespace media {
namespace mp4 {

FrameCENCInfo::FrameCENCInfo() {}
FrameCENCInfo::~FrameCENCInfo() {}

bool FrameCENCInfo::Parse(int iv_size, BufferReader* reader) {
  const int kEntrySize = 6;

  // Mandated by CENC spec
  RCHECK(iv_size == 8 || iv_size == 16);
  iv.resize(iv_size);

  uint16 subsample_count;
  RCHECK(reader->ReadVec(&iv, iv_size) &&
         reader->Read2(&subsample_count) &&
         reader->HasBytes(subsample_count * kEntrySize));
  subsamples.resize(subsample_count);

  for (int i = 0; i < subsample_count; i++) {
    RCHECK(reader->Read2(&subsamples[i].clear_size) &&
           reader->Read4(&subsamples[i].encrypted_size));
  }
  return true;
}

size_t FrameCENCInfo::GetTotalSize() const {
  size_t size = 0;
  for (size_t i = 0; i < subsamples.size(); i++) {
    size += subsamples[i].clear_size + subsamples[i].encrypted_size;
  }
  return size;
}

}  // namespace mp4
}  // namespace media
