/* Copyright (c) 2011 The WebM project authors. All Rights Reserved. */
/*  */
/* Use of this source code is governed by a BSD-style license */
/* that can be found in the LICENSE file in the root of the source */
/* tree. An additional intellectual property rights grant can be found */
/* in the file PATENTS.  All contributing project authors may */
/* be found in the AUTHORS file in the root of the source tree. */
#include "vpx/vpx_codec.h"
static const char* const cfg = "--target=x86_64-win64-vs14 --enable-external-build --disable-libyuv --enable-postproc --disable-examples --disable-tools --disable-docs --disable-unit-tests --enable-multithread --enable-runtime-cpu-detect --enable-vp9-temporal-denoising --disable-webm-io --disable-vp9-encoder --disable-vp8 --enable-pic --as=yasm --disable-avx512 --enable-vp9-highbitdepth";
const char *vpx_codec_build_config(void) {return cfg;}
