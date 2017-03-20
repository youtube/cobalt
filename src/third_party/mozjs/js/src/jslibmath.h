/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jslibmath_h
#define jslibmath_h

#include "mozilla/FloatingPoint.h"

#include <math.h>
#include "jsnum.h"

#if defined(STARBOARD)
#include "starboard/configuration.h"
#endif

/*
 * Use system provided math routines.
 */

#if defined(STARBOARD)
#if SB_HAS_QUIRK(COMPILER_SAYS_GNUC_BUT_ISNT)
#define ENABLE_COMPILER_SAYS_GNUC_BUT_ISNT_WORKAROUND
#endif
#endif

/* The right copysign function is not always named the same thing. */
// Check for quirk COMPILER_SAYS_GNUC_BUT_ISNT when picking copysign function.
#if defined(__GNUC__) && !defined(ENABLE_COMPILER_SAYS_GNUC_BUT_ISNT_WORKAROUND)
#define js_copysign __builtin_copysign
#elif defined _WIN32
#define js_copysign _copysign
#else
#define js_copysign copysign
#endif

/* Consistency wrapper for platform deviations in fmod() */
static inline double
js_fmod(double d, double d2)
{
#ifdef XP_WIN
    /*
     * Workaround MS fmod bug where 42 % (1/0) => NaN, not 42.
     * Workaround MS fmod bug where -0 % -N => 0, not -0.
     */
    if ((mozilla::IsFinite(d) && mozilla::IsInfinite(d2)) ||
        (d == 0 && mozilla::IsFinite(d2))) {
        return d;
    }
#endif
    return fmod(d, d2);
}

namespace js {

inline double
NumberDiv(double a, double b)
{
    if (b == 0) {
        if (a == 0 || mozilla::IsNaN(a)
#ifdef XP_WIN
            || mozilla::IsNaN(b) /* XXX MSVC miscompiles such that (NaN == 0) */
#endif
        )
            return js_NaN;

        if (mozilla::IsNegative(a) != mozilla::IsNegative(b))
            return js_NegativeInfinity;
        return js_PositiveInfinity;
    }

    return a / b;
}

inline double
NumberMod(double a, double b) {
    if (b == 0)
        return js_NaN;
    return js_fmod(a, b);
}

}

#endif /* jslibmath_h */
