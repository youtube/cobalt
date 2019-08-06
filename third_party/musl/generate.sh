#!/bin/bash

sed -f tools/mkalltypes.sed arch/aarch64/bits/alltypes.h.in  include/alltypes.h.in > arch/aarch64/bits/alltypes.h
sed -f tools/mkalltypes.sed arch/arm/bits/alltypes.h.in include/alltypes.h.in > arch/arm/bits/alltypes.h
sed -f tools/mkalltypes.sed arch/x86_64/bits/alltypes.h.in include/alltypes.h.in > arch/x86_64/bits/alltypes.h
sed -f tools/mkalltypes.sed arch/i386/bits/alltypes.h.in include/alltypes.h.in > arch/i386/bits/alltypes.h
sed -f tools/mkalltypes.sed arch/mips/bits/alltypes.h.in include/alltypes.h.in > arch/mips/bits/alltypes.h

