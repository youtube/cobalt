// NOLINT(legal/copyright)
#ifndef STARBOARD_DARWIN_INCLUDES_GLES2_GL2PLATFORM_H_
#define STARBOARD_DARWIN_INCLUDES_GLES2_GL2PLATFORM_H_

/* $Revision: 23328 $ on $Date:: 2013-10-02 02:28:28 -0700 #$ */

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

#ifndef GL_APICALL
#define GL_APICALL KHRONOS_APICALL
#endif

#ifndef GL_APIENTRY
#define GL_APIENTRY KHRONOS_APIENTRY
#endif

#endif  // STARBOARD_DARWIN_INCLUDES_GLES2_GL2PLATFORM_H_
