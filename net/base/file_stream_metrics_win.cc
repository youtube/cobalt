// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/file_stream_metrics.h"

#include <windows.h>

#include "base/basictypes.h"

namespace net {

namespace {

struct Range {
  int low;
  int high;
};

// The error range list is extracted from WinError.h.
//
// NOTE: The gaps between the ranges need to be recorded too.
// They will have odd-numbered buckets.
const Range kErrorRangeList[] = {
  { 0, 321 },  // 2.
  { 335, 371 },  // 4.
  { 383, 387 },  // 6.
  { 399, 404 },  // etc.
  { 415, 418 },
  { 431, 433 },
  { 447, 868 },
  { 994, 1471 },
  { 1500, 1513 },
  { 1536, 1553 },
  { 1601, 1654 },
  { 1700, 1834 },
  { 1898, 1938 },
  { 2000, 2024 },
  { 2048, 2085 },
  { 2108, 2110 },
  { 2202, 2203 },
  { 2250, 2251 },
  { 2401, 2405 },
  { 3000, 3021 },
  { 3950, 3951 },
  { 4000, 4007 },
  { 4050, 4066 },
  { 4096, 4116 },
  { 4200, 4215 },
  { 4300, 4353 },
  { 4390, 4395 },
  { 4500, 4501 },
  { 4864, 4905 },
  { 5001, 5090 },
  { 5890, 5953 },
  { 6000, 6023 },
  { 6118, 6119 },
  { 6200, 6201 },
  { 6600, 6649 },
  { 6700, 6732 },
  { 6800, 6856 },
  { 7001, 7071 },
  { 8001, 8018 },
  { 8192, 8263 },
  { 8301, 8640 },
  { 8704, 8705 },
  { 8960, 9053 },
  { 9216, 9218 },
  { 9263, 9276 },
  { 9472, 9506 },
  { 9550, 9573 },
  { 9600, 9622 },
  { 9650, 9656 },
  { 9688, 9723 },
  { 9750, 9754 },
  { 9800, 9802 },
  { 9850, 9853 },
  { 9900, 9907 },
  { 10000, 10072 },
  { 10091, 10113 },
  { 11001, 11034 },
  { 12288, 12335 },
  { 12544, 12559 },
  { 12595, 12597 },
  { 12801, 12803 },
  { 13000, 13026 },
  { 13800, 13933 },
  { 14000, 14111 },
  { 15000, 15039 },
  { 15080, 15086 },
  { 15100, 15109 },
  { 15200, 15208 },
  { 15250, 15251 },
  { 15299, 15302 },
  { 16385, 16436 },
  { 18432, 18454 },
  { 20480, 20486 },
  { 24577, 24607 },
  { 28673, 28698 },
  { 32790, 32816 },
  { 33281, 33322 },
  { 35005, 35024 },
  { 36000, 36004 },
  { 40010, 40011 },
  { 40067, 40069 },
  { 53248, 53293 },
  { 53376, 53382 },
  { 57344, 57360 },
  { 57377, 57394 },
  { 65535, 65536 }  // 2 * kNumErrorRanges.
};
const size_t kNumErrorRanges = ARRAYSIZE_UNSAFE(kErrorRangeList);

}  // namespace

// Windows has very many errors.  We're not interested in most of them, but we
// don't know which ones are significant.
// This function maps error ranges to specific buckets.
// If we get hits on the buckets, we can add those values to the values we
// record individually.
// If we get values *between* the buckets, we record those as buckets too.
int GetFileErrorUmaBucket(int error) {
  error = HRESULT_CODE(error);

  // This is a linear search, but of a short fixed-size array.
  // It also gets called infrequently, on errors.
  for (size_t n = 0; n < kNumErrorRanges; ++n) {
    if (error < kErrorRangeList[n].low)
      return (2 * (n + 1)) - 1;  // In gap before the range.
    if (error <= kErrorRangeList[n].high)
      return 2 * (n + 1);  // In the range.
  }

  // After the last bucket.
  return 2 * kNumErrorRanges + 1;
}

int MaxFileErrorUmaBucket() {
  return 2 * kNumErrorRanges + 2;
}

int MaxFileErrorUmaValue() {
  return kErrorRangeList[0].high + 1;
}

}  // namespace net
