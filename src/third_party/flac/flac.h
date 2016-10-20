// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_FLAC_FLAC_H_
#define THIRD_PARTY_FLAC_FLAC_H_
#pragma once

// This is a shim header to include the right flac headers.
// Use this instead of referencing the flac headers directly.

#ifdef STARBOARD
#include "starboard/configuration.h"
#  if SB_IS(BIG_ENDIAN)
#    define WORDS_BIGENDIAN 1
#  else
#    undef WORDS_BIGENDIAN
#  endif
#endif  // STARBOARD

#if defined(USE_SYSTEM_FLAC)
#include <FLAC/stream_encoder.h>
#else
#include "third_party/flac/include/FLAC/stream_encoder.h"
#endif

#endif  // THIRD_PARTY_FLAC_FLAC_H_
