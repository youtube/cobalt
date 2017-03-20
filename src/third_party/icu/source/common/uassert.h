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
*  Contains U_ASSERT macro
*
*    By default, U_ASSERT just wraps the C library assert macro.
*    By changing the definition here, the assert behavior for ICU can be changed
*    without affecting other non-ICU uses of the C library assert().
*
******************************************************************************
*/

#ifndef U_ASSERT_H
#define U_ASSERT_H
/* utypes.h is included to get the proper define for uint8_t */
#include "unicode/utypes.h"
#if defined(STARBOARD)
#include "starboard/configuration.h"
#endif
#if U_DEBUG
#if !defined(STARBOARD)
#   include <assert.h>
#endif
#   define U_ASSERT(exp) assert(exp)
#else
#   define U_ASSERT(exp) SB_UNREFERENCED_PARAMETER(exp)
#endif
#endif


