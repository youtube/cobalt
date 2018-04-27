/* Copyright (c) 2011 The WebM project authors. All Rights Reserved. */
/*  */
/* Use of this source code is governed by a BSD-style license */
/* that can be found in the LICENSE file in the root of the source */
/* tree. An additional intellectual property rights grant can be found */
/* in the file PATENTS.  All contributing project authors may */
/* be found in the AUTHORS file in the root of the source tree. */
#include "vpx/vpx_codec.h"
static const char* const cfg = "--target=armv7-linux-gcc --disable-vp8 --disable-examples --disable-webm-io --disable-vp9-encoder --disable-avx --disable-sse4_1 --disable-ssse3 --disable-mmx --disable-sse --disable-sse2 --disable-sse3 --disable-runtime_cpu_detect";
const char *vpx_codec_build_config(void) {return cfg;}
