// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 2002-2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*
* File uassert.h
*
*  Contains the U_ASSERT and UPRV_UNREACHABLE macros
*
******************************************************************************
*/
#ifndef U_ASSERT_H
#define U_ASSERT_H

/* utypes.h is included to get the proper define for uint8_t */
#include "unicode/utypes.h"
<<<<<<< HEAD
#if defined(STARBOARD)
#include "starboard/configuration.h"
#endif
=======
/* for abort */
#include <stdlib.h>

/**
 * \def U_ASSERT
 * By default, U_ASSERT just wraps the C library assert macro.
 * By changing the definition here, the assert behavior for ICU can be changed
 * without affecting other non - ICU uses of the C library assert().
*/
>>>>>>> 047a7134fa7a3ed5d506179d439db144bf326e70
#if U_DEBUG
#if !defined(STARBOARD)
#   include <assert.h>
#endif
#   define U_ASSERT(exp) assert(exp)
#elif U_CPLUSPLUS_VERSION
#   define U_ASSERT(exp) (void)0
#else
#   define U_ASSERT(exp)
#endif

/**
 * \def UPRV_UNREACHABLE
 * This macro is used to unconditionally abort if unreachable code is ever executed.
 * @internal
*/
#if defined(UPRV_UNREACHABLE)
    // Use the predefined value.
#else
#   define UPRV_UNREACHABLE abort()
#endif

#endif
