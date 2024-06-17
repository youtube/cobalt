#ifndef __gl2platform_h_
#define __gl2platform_h_

/*
** Copyright 2017-2020 The Khronos Group Inc.
** SPDX-License-Identifier: Apache-2.0
*/

/* Platform-specific types and definitions for OpenGL ES 2.X  gl2.h
 *
 * Adopters may modify khrplatform.h and this file to suit their platform.
 * Please contribute modifications back to Khronos as pull requests on the
 * public github repository:
 *      https://github.com/KhronosGroup/OpenGL-Registry
 */

#include <KHR/khrplatform.h>

#if defined(STARBOARD)
// Don't trust any previous definitions of GL_APICALL. We really want nothing.
#undef GL_APICALL
#define GL_APICALL  /* nothing */
#else
#ifndef GL_APICALL
#define GL_APICALL  KHRONOS_APICALL
#endif
#endif  // defined(STARBOARD)

#ifndef GL_APIENTRY
#define GL_APIENTRY KHRONOS_APIENTRY
#endif

#endif /* __gl2platform_h_ */
