#ifndef __gl2platform_h_
#define __gl2platform_h_

/* $Revision: 10602 $ on $Date:: 2010-03-04 22:35:34 -0800 #$ */

/*
 * This document is licensed under the SGI Free Software B License Version
 * 2.0. For details, see http://oss.sgi.com/projects/FreeB/ .
 */

/* Platform-specific types and definitions for OpenGL ES 2.X  gl2.h
 *
 * Adopters may modify khrplatform.h and this file to suit their platform.
 * You are encouraged to submit all modifications to the Khronos group so that
 * they can be included in future versions of this file.  Please submit changes
 * by sending them to the public Khronos Bugzilla (http://khronos.org/bugzilla)
 * by filing a bug against product "OpenGL-ES" component "Registry".
 */

#include <KHR/khrplatform.h>

#if defined(COMPONENT_BUILD)
#if defined(WIN32)
#if defined(GLES2_C_LIB_IMPLEMENTATION)
#define GL_APICALL __declspec(dllexport)
#else
#define GL_APICALL __declspec(dllimport)
#endif  /* defined(GLES2_C_LIB_IMPLEMENTATION) */
#else /* defined(WIN32) */
#define GL_APICALL __attribute__((visibility("default")))
#endif
#else
#   define GL_APICALL
#endif

#ifndef GL_APIENTRY
#define GL_APIENTRY KHRONOS_APIENTRY
#endif

#undef GL_APIENTRY
#define GL_APIENTRY

#endif /* __gl2platform_h_ */
