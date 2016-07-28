// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/file_stream_metrics.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"

namespace net {

namespace {

const char* FileErrorSourceStrings[] = {
  "OPEN",
  "WRITE",
  "READ",
  "SEEK",
  "FLUSH",
  "SET_EOF",
  "GET_SIZE"
};

COMPILE_ASSERT(ARRAYSIZE_UNSAFE(FileErrorSourceStrings) ==
                   FILE_ERROR_SOURCE_COUNT,
               file_error_source_enum_has_changed);

void RecordFileErrorTypeCount(FileErrorSource source) {
  UMA_HISTOGRAM_ENUMERATION(
      "Net.FileErrorType_Counts", source, FILE_ERROR_SOURCE_COUNT);
}

}  // namespace

void RecordFileError(int error, FileErrorSource source, bool record) {
  LOG(ERROR) << " " << __FUNCTION__ << "()"
             << " error = " << error
             << " source = " << source
             << " record = " << record;

  if (!record)
    return;

  RecordFileErrorTypeCount(source);

  int bucket = GetFileErrorUmaBucket(error);

  // Fixed values per platform.
  static const int max_bucket = MaxFileErrorUmaBucket();
  static const int max_error = MaxFileErrorUmaValue();

  switch(source) {
    case FILE_ERROR_SOURCE_OPEN:
      UMA_HISTOGRAM_ENUMERATION("Net.FileError_Open", error, max_error);
      UMA_HISTOGRAM_ENUMERATION("Net.FileErrorRange_Open", bucket, max_bucket);
      break;

    case FILE_ERROR_SOURCE_WRITE:
      UMA_HISTOGRAM_ENUMERATION("Net.FileError_Write", error, max_error);
      UMA_HISTOGRAM_ENUMERATION("Net.FileErrorRange_Write", bucket, max_bucket);
      break;

    case FILE_ERROR_SOURCE_READ:
      UMA_HISTOGRAM_ENUMERATION("Net.FileError_Read", error, max_error);
      UMA_HISTOGRAM_ENUMERATION("Net.FileErrorRange_Read", bucket, max_bucket);
      break;

    case FILE_ERROR_SOURCE_SEEK:
      UMA_HISTOGRAM_ENUMERATION("Net.FileError_Seek", error, max_error);
      UMA_HISTOGRAM_ENUMERATION("Net.FileErrorRange_Seek", bucket, max_bucket);
      break;

    case FILE_ERROR_SOURCE_FLUSH:
      UMA_HISTOGRAM_ENUMERATION("Net.FileError_Flush", error, max_error);
      UMA_HISTOGRAM_ENUMERATION("Net.FileErrorRange_Flush", bucket, max_bucket);
      break;

    case FILE_ERROR_SOURCE_SET_EOF:
      UMA_HISTOGRAM_ENUMERATION("Net.FileError_SetEof", error, max_error);
      UMA_HISTOGRAM_ENUMERATION("Net.FileErrorRange_SetEof", bucket,
                                max_bucket);
      break;

    case FILE_ERROR_SOURCE_GET_SIZE:
      UMA_HISTOGRAM_ENUMERATION("Net.FileError_GetSize", error, max_error);
      UMA_HISTOGRAM_ENUMERATION("Net.FileErrorRange_GetSize", bucket,
                                max_bucket);
      break;

    default:
      break;
  }
}

const char* GetFileErrorSourceName(FileErrorSource source) {
  DCHECK_NE(FILE_ERROR_SOURCE_COUNT, source);
  return FileErrorSourceStrings[source];
}

}  // namespace net
