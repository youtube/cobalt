// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_TEST_DATA_UTIL_H_
#define MEDIA_BASE_TEST_DATA_UTIL_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/data_buffer.h"

namespace media {

// Reads a test file from media/test/data directory and stores it in
// a scoped_array.
//
//  |name| - The name of the file.
//  |buffer| - The contents of the file.
//  |size| - The size of the buffer.
void ReadTestDataFile(const std::string& name,
                      scoped_array<uint8>* buffer,
                      int* size);

// Reads a test file from media/test/data directory and stores it in
// a Buffer.
//
//  |name| - The name of the file.
//  |buffer| - The contents of the file.
void ReadTestDataFile(const std::string& name, scoped_refptr<Buffer>* buffer);

}  // namespace media

#endif  // MEDIA_BASE_TEST_DATA_UTIL_H_
