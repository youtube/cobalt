/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_AsmJS_h
#define jit_AsmJS_h

#if defined(JS_CPU_MIPS)
#include <stddef.h>
#include "vm/ObjectImpl.h"
#endif  // defined(JS_CPU_MIPS)

#ifdef XP_MACOSX
# include <pthread.h>
# include <mach/mach.h>
#endif

namespace js {

class ScriptSource;
class SPSProfiler;
class AsmJSModule;
namespace frontend { struct TokenStream; struct ParseNode; }
namespace jit { class MIRGenerator; class LIRGraph; }

// Called after parsing a function 'fn' which contains the "use asm" directive.
// This function performs type-checking and code-generation. If type-checking
// succeeds, the generated native function is assigned to |moduleFun|.
// Otherwise, a warning will be emitted and |moduleFun| is left unchanged. The
// function returns 'false' only if a real JS semantic error (probably OOM) is
// pending.
extern bool
CompileAsmJS(JSContext *cx, frontend::TokenStream &ts, frontend::ParseNode *fn,
             const CompileOptions &options,
             ScriptSource *scriptSource, uint32_t bufStart, uint32_t bufEnd,
             MutableHandleFunction moduleFun);

// Called for any "use asm" function which successfully typechecks. This
// function performs the validation and dynamic linking of a module to its
// given arguments. If validation succeeds, the module's return value (its
// exports) are returned via |vp|.  Otherwise, there was a validation error and
// execution fall back to the usual path (bytecode generation, interpretation,
// etc). The function returns 'false' only if a real JS semantic error (OOM or
// exception thrown when executing GetProperty on the arguments) is pending.
extern JSBool
LinkAsmJS(JSContext *cx, unsigned argc, JS::Value *vp);

// The js::Native for the functions nested in an asm.js module. Calling this
// native will trampoline into generated code.
extern JSBool
CallAsmJS(JSContext *cx, unsigned argc, JS::Value *vp);

// Force any currently-executing asm.js code to call
// js_HandleExecutionInterrupt.
void
TriggerOperationCallbackForAsmJSCode(JSRuntime *rt);

// The JSRuntime maintains a stack of AsmJSModule activations. An "activation"
// of module A is an initial call from outside A into a function inside A,
// followed by a sequence of calls inside A, and terminated by a call that
// leaves A. The AsmJSActivation stack serves three purposes:
//  - record the correct cx to pass to VM calls from asm.js;
//  - record enough information to pop all the frames of an activation if an
//    exception is thrown;
//  - record the information necessary for asm.js signal handlers to safely
//    recover from (expected) out-of-bounds access, the operation callback,
//    stack overflow, division by zero, etc.
class AsmJSActivation
{
    JSContext *cx_;
    const AsmJSModule &module_;
    AsmJSActivation *prev_;
    void *errorRejoinSP_;
    SPSProfiler *profiler_;
    void *resumePC_;

  public:
    AsmJSActivation(JSContext *cx, const AsmJSModule &module);
    ~AsmJSActivation();

    JSContext *cx() { return cx_; }
    const AsmJSModule &module() const { return module_; }

    // Read by JIT code:
    static unsigned offsetOfContext() { return offsetof(AsmJSActivation, cx_); }
    static unsigned offsetOfResumePC() { return offsetof(AsmJSActivation, resumePC_); }

    // Initialized by JIT code:
    static unsigned offsetOfErrorRejoinSP() { return offsetof(AsmJSActivation, errorRejoinSP_); }

    // Set from SIGSEGV handler:
    void setResumePC(void *pc) { resumePC_ = pc; }
};

// The assumed page size; dynamically checked in CompileAsmJS.
const size_t AsmJSPageSize = 4096;

// The asm.js spec requires that the ArrayBuffer's byteLength be a multiple of 4096.
static const size_t AsmJSAllocationGranularity = 4096;

#ifdef JS_CPU_X64
// On x64, the internal ArrayBuffer data array is inflated to 4GiB (only the
// byteLength portion of which is accessible) so that out-of-bounds accesses
// (made using a uint32 index) are guaranteed to raise a SIGSEGV.
static const size_t AsmJSBufferProtectedSize = 4 * 1024ULL * 1024ULL * 1024ULL;
#endif

#ifdef XP_MACOSX
class AsmJSMachExceptionHandler
{
    bool installed_;
    pthread_t thread_;
    mach_port_t port_;

    void release();

  public:
    AsmJSMachExceptionHandler();
    ~AsmJSMachExceptionHandler() { release(); }
    mach_port_t port() const { return port_; }
    bool installed() const { return installed_; }
    bool install(JSRuntime *rt);
    void clearCurrentThread();
    void setCurrentThread();
};
#endif

struct DependentAsmJSModuleExit
{
    const AsmJSModule *module;
    size_t exitIndex;

    DependentAsmJSModuleExit(const AsmJSModule *module, size_t exitIndex)
      : module(module),
        exitIndex(exitIndex)
    { }
};

// Struct type for passing parallel compilation data between the main thread
// and compilation workers.
struct AsmJSParallelTask
{
    LifoAlloc lifo;         // Provider of all heap memory used for compilation.

    uint32_t funcNum;       // Index |i| of function in |Module.function(i)|.
    jit::MIRGenerator *mir; // Passed from main thread to worker.
    jit::LIRGraph *lir;     // Passed from worker to main thread.
    unsigned compileTime;

    AsmJSParallelTask(size_t defaultChunkSize)
      : lifo(defaultChunkSize),
        funcNum(0), mir(NULL), lir(NULL), compileTime(0)
    { }

    void init(uint32_t newFuncNum, jit::MIRGenerator *newMir) {
        funcNum = newFuncNum;
        mir = newMir;
        lir = NULL;
    }
};

// Returns true if the given native is the one that is used to implement asm.js
// module functions.
#ifdef JS_ION
extern bool
IsAsmJSModuleNative(js::Native native);
#else
inline bool
IsAsmJSModuleNative(js::Native native)
{
    return false;
}
#endif

// Exposed for shell testing:

// Return whether asm.js optimization is inhibitted by the platform or
// dynamically disabled:
extern JSBool
IsAsmJSCompilationAvailable(JSContext *cx, unsigned argc, Value *vp);

// Return whether the given value is a function containing "use asm" that has
// been validated according to the asm.js spec.
extern JSBool
IsAsmJSModule(JSContext *cx, unsigned argc, Value *vp);

// Return whether the given value is a nested function in an asm.js module that
// has been both compile- and link-time validated.
extern JSBool
IsAsmJSFunction(JSContext *cx, unsigned argc, Value *vp);

} // namespace js

#endif /* jit_AsmJS_h */
