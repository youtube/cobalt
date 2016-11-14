/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/ParallelFunctions.h"

#include "jit/IonSpewer.h"
#include "vm/Interpreter.h"

#include "jscompartmentinlines.h"
#include "jsstrinlines.h"
#include "vm/Interpreter-inl.h"

using namespace js;
using namespace jit;

using parallel::Spew;
using parallel::SpewOps;
using parallel::SpewBailouts;
using parallel::SpewBailoutIR;

// Load the current thread context.
ForkJoinSlice *
jit::ParForkJoinSlice()
{
    return ForkJoinSlice::Current();
}

// ParNewGCThing() is called in place of NewGCThing() when executing
// parallel code.  It uses the ArenaLists for the current thread and
// allocates from there.
JSObject *
jit::ParNewGCThing(gc::AllocKind allocKind)
{
    ForkJoinSlice *slice = ForkJoinSlice::Current();
    uint32_t thingSize = (uint32_t)gc::Arena::thingSize(allocKind);
    return gc::NewGCThing<JSObject, NoGC>(slice, allocKind, thingSize, gc::DefaultHeap);
}

// Check that the object was created by the current thread
// (and hence is writable).
bool
jit::ParWriteGuard(ForkJoinSlice *slice, JSObject *object)
{
    JS_ASSERT(ForkJoinSlice::Current() == slice);
    return !IsInsideNursery(slice->runtime(), object) &&
           slice->allocator()->arenas.containsArena(slice->runtime(), object->arenaHeader());
}

#ifdef DEBUG
static void
printTrace(const char *prefix, struct IonLIRTraceData *cached)
{
    fprintf(stderr, "%s / Block %3u / LIR %3u / Mode %u / LIR %s\n",
            prefix,
            cached->bblock, cached->lir, cached->execModeInt, cached->lirOpName);
}

struct IonLIRTraceData seqTraceData;
#endif

void
jit::TraceLIR(uint32_t bblock, uint32_t lir, uint32_t execModeInt,
              const char *lirOpName, const char *mirOpName,
              JSScript *script, jsbytecode *pc)
{
#ifdef DEBUG
    static enum { NotSet, All, Bailouts } traceMode;

    // If you set IONFLAGS=trace, this function will be invoked before every LIR.
    //
    // You can either modify it to do whatever you like, or use gdb scripting.
    // For example:
    //
    // break ParTrace
    // commands
    // continue
    // exit

    if (traceMode == NotSet) {
        // Racy, but that's ok.
        const char* env = "";
        if (strstr(env, "trace-all"))
            traceMode = All;
        else
            traceMode = Bailouts;
    }

    IonLIRTraceData *cached;
    if (execModeInt == 0)
        cached = &seqTraceData;
    else
        cached = &ForkJoinSlice::Current()->traceData;

    if (bblock == 0xDEADBEEF) {
        if (execModeInt == 0)
            printTrace("BAILOUT", cached);
        else
            SpewBailoutIR(cached->bblock, cached->lir,
                          cached->lirOpName, cached->mirOpName,
                          cached->script, cached->pc);
    }

    cached->bblock = bblock;
    cached->lir = lir;
    cached->execModeInt = execModeInt;
    cached->lirOpName = lirOpName;
    cached->mirOpName = mirOpName;
    cached->script = script;
    cached->pc = pc;

    if (traceMode == All)
        printTrace("Exec", cached);
#endif
}

bool
jit::ParCheckOverRecursed(ForkJoinSlice *slice)
{
    JS_ASSERT(ForkJoinSlice::Current() == slice);
    int stackDummy_;

    // When an interrupt is triggered, the main thread stack limit is
    // overwritten with a sentinel value that brings us here.
    // Therefore, we must check whether this is really a stack overrun
    // and, if not, check whether an interrupt is needed.
    //
    // When not on the main thread, we don't overwrite the stack
    // limit, but we do still call into this routine if the interrupt
    // flag is set, so we still need to double check.

    uintptr_t realStackLimit;
    if (slice->isMainThread())
        realStackLimit = js::GetNativeStackLimit(slice->runtime());
    else
        realStackLimit = slice->perThreadData->ionStackLimit;

    if (!JS_CHECK_STACK_SIZE(realStackLimit, &stackDummy_)) {
        slice->bailoutRecord->setCause(ParallelBailoutOverRecursed,
                                       NULL, NULL, NULL);
        return false;
    }

    return ParCheckInterrupt(slice);
}

bool
jit::ParCheckInterrupt(ForkJoinSlice *slice)
{
    JS_ASSERT(ForkJoinSlice::Current() == slice);
    bool result = slice->check();
    if (!result) {
        // Do not set the cause here.  Either it was set by this
        // thread already by some code that then triggered an abort,
        // or else we are just picking up an abort from some other
        // thread.  Either way we have nothing useful to contribute so
        // we might as well leave our bailout case unset.
        return false;
    }
    return true;
}

void
jit::ParDumpValue(Value *v)
{
#ifdef DEBUG
    js_DumpValue(*v);
#endif
}

JSObject*
jit::ParPush(ParPushArgs *args)
{
    // It is awkward to have the MIR pass the current slice in, so
    // just fetch it from TLS.  Extending the array is kind of the
    // slow path anyhow as it reallocates the elements vector.
    ForkJoinSlice *slice = js::ForkJoinSlice::Current();
    JSObject::EnsureDenseResult res =
        args->object->parExtendDenseElements(slice, &args->value, 1);
    if (res != JSObject::ED_OK)
        return NULL;
    return args->object;
}

JSObject *
jit::ParExtendArray(ForkJoinSlice *slice, JSObject *array, uint32_t length)
{
    JSObject::EnsureDenseResult res =
        array->parExtendDenseElements(slice, NULL, length);
    if (res != JSObject::ED_OK)
        return NULL;
    return array;
}

#define PAR_RELATIONAL_OP(OP, EXPECTED)                                         \
do {                                                                            \
    /* Optimize for two int-tagged operands (typical loop control). */          \
    if (lhs.isInt32() && rhs.isInt32()) {                                       \
        *res = (lhs.toInt32() OP rhs.toInt32()) == EXPECTED;                    \
    } else if (lhs.isNumber() && rhs.isNumber()) {                              \
        double l = lhs.toNumber(), r = rhs.toNumber();                          \
        *res = (l OP r) == EXPECTED;                                            \
    } else if (lhs.isBoolean() && rhs.isBoolean()) {                            \
        bool l = lhs.toBoolean();                                               \
        bool r = rhs.toBoolean();                                               \
        *res = (l OP r) == EXPECTED;                                            \
    } else if (lhs.isBoolean() && rhs.isNumber()) {                             \
        bool l = lhs.toBoolean();                                               \
        double r = rhs.toNumber();                                              \
        *res = (l OP r) == EXPECTED;                                            \
    } else if (lhs.isNumber() && rhs.isBoolean()) {                             \
        double l = lhs.toNumber();                                              \
        bool r = rhs.toBoolean();                                               \
        *res = (l OP r) == EXPECTED;                                            \
    } else {                                                                    \
        int32_t vsZero;                                                         \
        ParallelResult ret = ParCompareMaybeStrings(slice, lhs, rhs, &vsZero);  \
        if (ret != TP_SUCCESS)                                                  \
            return ret;                                                         \
        *res = (vsZero OP 0) == EXPECTED;                                       \
    }                                                                           \
    return TP_SUCCESS;                                                          \
} while(0)

static ParallelResult
ParCompareStrings(ForkJoinSlice *slice, HandleString str1,
                  HandleString str2, int32_t *res)
{
    if (!str1->isLinear())
        return TP_RETRY_SEQUENTIALLY;
    if (!str2->isLinear())
        return TP_RETRY_SEQUENTIALLY;
    JSLinearString &linearStr1 = str1->asLinear();
    JSLinearString &linearStr2 = str2->asLinear();
    if (!CompareChars(linearStr1.chars(), linearStr1.length(),
                      linearStr2.chars(), linearStr2.length(),
                      res))
        return TP_FATAL;

    return TP_SUCCESS;
}

static ParallelResult
ParCompareMaybeStrings(ForkJoinSlice *slice,
                       HandleValue v1,
                       HandleValue v2,
                       int32_t *res)
{
    if (!v1.isString())
        return TP_RETRY_SEQUENTIALLY;
    if (!v2.isString())
        return TP_RETRY_SEQUENTIALLY;
    RootedString str1(slice->perThreadData, v1.toString());
    RootedString str2(slice->perThreadData, v2.toString());
    return ParCompareStrings(slice, str1, str2, res);
}

template<bool Equal>
ParallelResult
ParLooselyEqualImpl(ForkJoinSlice *slice, MutableHandleValue lhs, MutableHandleValue rhs, JSBool *res)
{
    PAR_RELATIONAL_OP(==, Equal);
}

ParallelResult
js::jit::ParLooselyEqual(ForkJoinSlice *slice, MutableHandleValue lhs, MutableHandleValue rhs, JSBool *res)
{
    return ParLooselyEqualImpl<true>(slice, lhs, rhs, res);
}

ParallelResult
js::jit::ParLooselyUnequal(ForkJoinSlice *slice, MutableHandleValue lhs, MutableHandleValue rhs, JSBool *res)
{
    return ParLooselyEqualImpl<false>(slice, lhs, rhs, res);
}

template<bool Equal>
ParallelResult
ParStrictlyEqualImpl(ForkJoinSlice *slice, MutableHandleValue lhs, MutableHandleValue rhs, JSBool *res)
{
    if (lhs.isNumber()) {
        if (rhs.isNumber()) {
            *res = (lhs.toNumber() == rhs.toNumber()) == Equal;
            return TP_SUCCESS;
        }
    } else if (lhs.isBoolean()) {
        if (rhs.isBoolean()) {
            *res = (lhs.toBoolean() == rhs.toBoolean()) == Equal;
            return TP_SUCCESS;
        }
    } else if (lhs.isNull()) {
        if (rhs.isNull()) {
            *res = Equal;
            return TP_SUCCESS;
        }
    } else if (lhs.isUndefined()) {
        if (rhs.isUndefined()) {
            *res = Equal;
            return TP_SUCCESS;
        }
    } else if (lhs.isObject()) {
        if (rhs.isObject()) {
            *res = (lhs.toObjectOrNull() == rhs.toObjectOrNull()) == Equal;
            return TP_SUCCESS;
        }
    } else if (lhs.isString()) {
        if (rhs.isString())
            return ParLooselyEqualImpl<Equal>(slice, lhs, rhs, res);
    }

    *res = false;
    return TP_SUCCESS;
}

ParallelResult
js::jit::ParStrictlyEqual(ForkJoinSlice *slice, MutableHandleValue lhs, MutableHandleValue rhs, JSBool *res)
{
    return ParStrictlyEqualImpl<true>(slice, lhs, rhs, res);
}

ParallelResult
js::jit::ParStrictlyUnequal(ForkJoinSlice *slice, MutableHandleValue lhs, MutableHandleValue rhs, JSBool *res)
{
    return ParStrictlyEqualImpl<false>(slice, lhs, rhs, res);
}

ParallelResult
js::jit::ParLessThan(ForkJoinSlice *slice, MutableHandleValue lhs, MutableHandleValue rhs, JSBool *res)
{
    PAR_RELATIONAL_OP(<, true);
}

ParallelResult
js::jit::ParLessThanOrEqual(ForkJoinSlice *slice, MutableHandleValue lhs, MutableHandleValue rhs, JSBool *res)
{
    PAR_RELATIONAL_OP(<=, true);
}

ParallelResult
js::jit::ParGreaterThan(ForkJoinSlice *slice, MutableHandleValue lhs, MutableHandleValue rhs, JSBool *res)
{
    PAR_RELATIONAL_OP(>, true);
}

ParallelResult
js::jit::ParGreaterThanOrEqual(ForkJoinSlice *slice, MutableHandleValue lhs, MutableHandleValue rhs, JSBool *res)
{
    PAR_RELATIONAL_OP(>=, true);
}

template<bool Equal>
ParallelResult
ParStringsEqualImpl(ForkJoinSlice *slice, HandleString lhs, HandleString rhs, JSBool *res)
{
    int32_t vsZero;
    ParallelResult ret = ParCompareStrings(slice, lhs, rhs, &vsZero);
    if (ret != TP_SUCCESS)
        return ret;
    *res = (vsZero == 0) == Equal;
    return TP_SUCCESS;
}

ParallelResult
js::jit::ParStringsEqual(ForkJoinSlice *slice, HandleString v1, HandleString v2, JSBool *res)
{
    return ParStringsEqualImpl<true>(slice, v1, v2, res);
}

ParallelResult
js::jit::ParStringsUnequal(ForkJoinSlice *slice, HandleString v1, HandleString v2, JSBool *res)
{
    return ParStringsEqualImpl<false>(slice, v1, v2, res);
}

void
jit::ParallelAbort(ParallelBailoutCause cause,
                   JSScript *outermostScript,
                   JSScript *currentScript,
                   jsbytecode *bytecode)
{
    // Spew before asserts to help with diagnosing failures.
    Spew(SpewBailouts,
         "Parallel abort with cause %d in %p:%s:%d "
         "(%p:%s:%d at line %d)",
         cause,
         outermostScript, outermostScript->filename(), outermostScript->lineno,
         currentScript, currentScript->filename(), currentScript->lineno,
         (currentScript ? PCToLineNumber(currentScript, bytecode) : 0));

    JS_ASSERT(InParallelSection());
    JS_ASSERT(outermostScript != NULL);
    JS_ASSERT(currentScript != NULL);
    JS_ASSERT(outermostScript->hasParallelIonScript());

    ForkJoinSlice *slice = ForkJoinSlice::Current();

    JS_ASSERT(slice->bailoutRecord->depth == 0);
    slice->bailoutRecord->setCause(cause, outermostScript,
                                   currentScript, bytecode);
}

void
jit::PropagateParallelAbort(JSScript *outermostScript,
                            JSScript *currentScript)
{
    Spew(SpewBailouts,
         "Propagate parallel abort via %p:%s:%d (%p:%s:%d)",
         outermostScript, outermostScript->filename(), outermostScript->lineno,
         currentScript, currentScript->filename(), currentScript->lineno);

    JS_ASSERT(InParallelSection());
    JS_ASSERT(outermostScript->hasParallelIonScript());

    outermostScript->parallelIonScript()->setHasUncompiledCallTarget();

    ForkJoinSlice *slice = ForkJoinSlice::Current();
    if (currentScript)
        slice->bailoutRecord->addTrace(currentScript, NULL);
}

void
jit::ParCallToUncompiledScript(JSFunction *func)
{
    JS_ASSERT(InParallelSection());

#ifdef DEBUG
    static const int max_bound_function_unrolling = 5;

    if (func->hasScript()) {
        JSScript *script = func->nonLazyScript();
        Spew(SpewBailouts, "Call to uncompiled script: %p:%s:%d",
             script, script->filename(), script->lineno);
    } else if (func->isInterpretedLazy()) {
        Spew(SpewBailouts, "Call to uncompiled lazy script");
    } else if (func->isBoundFunction()) {
        int depth = 0;
        JSFunction *target = &func->getBoundFunctionTarget()->as<JSFunction>();
        while (depth < max_bound_function_unrolling) {
            if (target->hasScript())
                break;
            if (target->isBoundFunction())
                target = &target->getBoundFunctionTarget()->as<JSFunction>();
            depth--;
        }
        if (target->hasScript()) {
            JSScript *script = target->nonLazyScript();
            Spew(SpewBailouts, "Call to bound function leading (depth: %d) to script: %p:%s:%d",
                 depth, script, script->filename(), script->lineno);
        } else {
            Spew(SpewBailouts, "Call to bound function (excessive depth: %d)", depth);
        }
    } else {
        JS_NOT_REACHED("ParCall'ed functions must have scripts or be ES6 bound functions.");
    }
#endif
}

ParallelResult
jit::InitRestParameter(ForkJoinSlice *slice, uint32_t length, Value *rest,
                       HandleObject templateObj, HandleObject res,
                       MutableHandleObject out)
{
    // In parallel execution, we should always have succeeded in allocation
    // before this point. We can do the allocation here like in the sequential
    // path, but duplicating the initGCThing logic is too tedious.
    JS_ASSERT(res);
    JS_ASSERT(res->isArray());
    JS_ASSERT(!res->getDenseInitializedLength());
    JS_ASSERT(res->type() == templateObj->type());
    // See note in visitRest in ParallelArrayAnalysis.
    JS_ASSERT(res->type()->unknownProperties());

    if (length) {
        JSObject::EnsureDenseResult edr = res->parExtendDenseElements(slice, rest, length);
        if (edr != JSObject::ED_OK)
            return TP_FATAL;
    }

    out.set(res);
    return TP_SUCCESS;
}
