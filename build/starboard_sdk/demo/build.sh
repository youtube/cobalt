#!/bin/bash -x 
g++ \
glclear.cc \
main.cc \
-DSTARBOARD \
-DSTARBOARD_CONFIGURATION_INCLUDE=\"../platform_config/linux-x64x11/configuration_public.h\" \
-DSB_API_VERSION=16 \
-DSB_IS_ARCH_X64=1 \
-DSB_IS_64_BIT=1 \
-DSB_IS_LITTLE_ENDIAN=1 \
-DSB_SIZE_OF_LONG=8 \
-DSB_SIZE_OF_POINTER=8 \
-fsanitize=address \
-I ../include ../lib/libstarboard.so.16 \
-o glclear

