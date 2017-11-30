# Verify Trace Members

## Overview

`verify-trace-members` is a tool to verify correct usage of Cobalt's tracer members system.
See [***REMOVED***cobalt-trace-members](***REMOVED***cobalt-trace-members) for further details.

## Usage

`verify-trace-members` consists of two components:

1.  A clang based static analysis tool, that produces and inspects an AST, and outputs suspected
TraceMembers issues that it finds.
2.  A python wrapper of component 1, that is responsible for running 1 for multiple translation
units, and filtering its output.

Before you can run the tool, both gyp and ninja (for the all target) must be run for linux-x64x11.
Why?
Because in order to properly comprehend C++ in Cobalt, we must know the exact commands Clang
is invoked with when building it.
Ninja must be run because of generated headers.

Then, you can run `cobalt/tools/verify-trace-members/verify-trace-members.py` from `src/`, which
will output suspected TraceMembers issues.

## Build instructions

Follow the instructions available [here](https://clang.llvm.org/docs/LibASTMatchersTutorial.html).
You will have to check out (all hashes are from the git mirrors):

* llvm (last built against f7f7143561)
* clang (last built against 0caffbcfca)
* clang-tools-extra (last built against 7007ecbdf8)

Just add VerifyTraceMembers.cpp in place of LoopConvert.cpp.
The only important extra step not in the tutorial is to run cmake for the final binary with
`-DCMAKE_BUILD_TYPE=Release`.
