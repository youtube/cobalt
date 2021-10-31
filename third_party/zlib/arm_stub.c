/* arm_stub.c -- stub implementations
* Copyright (C) 2020 Intel Corporation
* For conditions of distribution and use, see copyright notice in zlib.h
*/
#include "arm_features.h"

#include <assert.h>

#include "zutil.h"
#include "starboard/common/log.h"

#if defined(STARBOARD)
#include "contrib/optimizations/slide_hash_neon.h"
#endif

int ZLIB_INTERNAL arm_cpu_enable_crc32 = 0;
#if defined(STARBOARD)
int ZLIB_INTERNAL arm_cpu_enable_neon = 0;

void ZLIB_INTERNAL neon_slide_hash(Posf *head, Posf *prev,
                                   const unsigned short w_size,
                                   const uInt hash_size)
{
    SB_NOTREACHED();
}
#endif
