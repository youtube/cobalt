/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdarg.h>
#if defined(__linux__)
# include <unistd.h>
#endif

#include "PerfSpewer.h"
#include "IonSpewer.h"
#include "LIR.h"
#include "MIR.h"
#include "MIRGraph.h"
#include "LinearScan.h"
#include "RangeAnalysis.h"

using namespace js;
using namespace js::jit;

#define PERF_MODE_NONE  1
#define PERF_MODE_FUNC  2
#define PERF_MODE_BLOCK 3

#ifdef JS_ION_PERF
static uint32_t PerfMode = 0;

static bool PerfChecked = false;

void
js::jit::CheckPerf() {
    if (!PerfChecked) {
        const char *env = getenv("IONPERF");
        if (env == NULL) {
            PerfMode = PERF_MODE_NONE;
        } else if (!strcmp(env, "none")) {
            PerfMode = PERF_MODE_NONE;
        } else if (!strcmp(env, "block")) {
            PerfMode = PERF_MODE_BLOCK;
        } else if (!strcmp(env, "func")) {
            PerfMode = PERF_MODE_FUNC;
        } else {
            fprintf(stderr, "Use IONPERF=func to record at basic block granularity\n");
            fprintf(stderr, "Use IONPERF=block to record at basic block granularity\n");
            fprintf(stderr, "\n");
            fprintf(stderr, "Be advised that using IONPERF will cause all scripts\n");
            fprintf(stderr, "to be leaked.\n");
            exit(0);
        }
        PerfChecked = true;
    }
}

bool
js::jit::PerfBlockEnabled() {
    JS_ASSERT(PerfMode);
    return PerfMode == PERF_MODE_BLOCK;
}

bool
js::jit::PerfFuncEnabled() {
    JS_ASSERT(PerfMode);
    return PerfMode == PERF_MODE_FUNC;
}

#endif

uint32_t PerfSpewer::nextFunctionIndex = 0;

PerfSpewer::PerfSpewer()
  : fp_(NULL)
{
    if (!PerfEnabled())
        return;

#   if defined(__linux__)
    // perf expects its data to be in a file /tmp/perf-PID.map
    const ssize_t bufferSize = 256;
    char filenameBuffer[bufferSize];
    if (snprintf(filenameBuffer, bufferSize,
                 "/tmp/perf-%d.map",
                 getpid()) >= bufferSize)
        return;

    fp_ = fopen(filenameBuffer, "a");
    if (!fp_)
        return;
#   else
    fprintf(stderr, "Warning: PerfEnabled, but not running on linux\n");
#   endif
}

PerfSpewer::~PerfSpewer()
{
    if (fp_)
        fclose(fp_);
}

bool
PerfSpewer::startBasicBlock(MBasicBlock *blk,
                            MacroAssembler &masm)
{
    if (!PerfBlockEnabled() || !fp_)
        return true;

    const char *filename = blk->info().script()->filename();
    unsigned lineNumber, columnNumber;
    if (blk->pc()) {
        lineNumber = PCToLineNumber(blk->info().script(),
                                    blk->pc(),
                                    &columnNumber);
    } else {
        lineNumber = 0;
        columnNumber = 0;
    }
    Record r(filename, lineNumber, columnNumber, blk->id());
    masm.bind(&r.start);
    return basicBlocks_.append(r);
}

bool
PerfSpewer::endBasicBlock(MacroAssembler &masm)
{
    masm.bind(&basicBlocks_[basicBlocks_.length() - 1].end);
    return true;
}

void
PerfSpewer::writeProfile(JSScript *script,
                         IonCode *code,
                         MacroAssembler &masm)
{
    if (!fp_)
        return;

    uint32_t thisFunctionIndex = nextFunctionIndex++;

    if (PerfFuncEnabled()) {
        unsigned long size = (unsigned long) code->instructionsSize();
        if (size > 0) {
            fprintf(fp_,
                    "%zx %lx %s:%d: Func%02d\n",
                    reinterpret_cast<uintptr_t>(code->raw()),
                    size,
                    script->filename(),
                    script->lineno,
                    thisFunctionIndex);
        }
    } else if (PerfBlockEnabled()) {
        uintptr_t funcStart = uintptr_t(code->raw());
        uintptr_t funcEnd = funcStart + code->instructionsSize();

        uintptr_t cur = funcStart;
        for (uint32_t i = 0; i < basicBlocks_.length(); i++) {
            Record &r = basicBlocks_[i];

            uintptr_t blockStart = funcStart + masm.actualOffset(r.start.offset());
            uintptr_t blockEnd = funcStart + masm.actualOffset(r.end.offset());

            JS_ASSERT(cur <= blockStart);
            if (cur < blockStart) {
                fprintf(fp_,
                        "%zx %zx %s:%d: Func%02d-Block?\n",
                        cur, blockStart - cur,
                        script->filename(), script->lineno,
                        thisFunctionIndex);
            }
            cur = blockEnd;

            unsigned long size = blockEnd - blockStart;

            if (size > 0) {
                fprintf(fp_,
                        "%zx %lx %s:%d:%d: Func%02d-Block%d\n",
                        blockStart, size,
                        r.filename, r.lineNumber, r.columnNumber,
                        thisFunctionIndex, r.id);
            }
        }

        // Any stuff after the basic blocks is presumably OOL code,
        // which I do not currently categorize.
        JS_ASSERT(cur <= funcEnd);
        if (cur < funcEnd) {
            fprintf(fp_,
                    "%zx %zx %s:%d: Func%02d-OOL\n",
                    cur, funcEnd - cur,
                    script->filename(), script->lineno,
                    thisFunctionIndex);
        }
    }
}
