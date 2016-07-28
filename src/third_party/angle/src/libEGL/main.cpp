//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// main.cpp: DLL entry point and management of thread-local data.
#include "libEGL/main.h"

#include "common/debug.h"

static volatile DWORD currentTLS = TLS_OUT_OF_INDEXES;

static egl::Current* GetCurrent()
{
#if !defined(COMPONENT_BUILD)
    if (currentTLS == TLS_OUT_OF_INDEXES)
        currentTLS = TlsAlloc();
#endif

    egl::Current *current = (egl::Current*)TlsGetValue(currentTLS);

#if !defined(COMPONENT_BUILD)
    if (current == NULL)
    {
        current = (egl::Current*)calloc(1, sizeof(egl::Current));

        if (current)
        {
            TlsSetValue(currentTLS, current);

            current->error = EGL_SUCCESS;
            current->API = EGL_OPENGL_ES_API;
            current->display = EGL_NO_DISPLAY;
            current->drawSurface = EGL_NO_SURFACE;
            current->readSurface = EGL_NO_SURFACE;
        }
    }
#endif

    return current;
}

#if defined(__cplusplus_winrt)
// Turns off a harmless warning that happens when compiling with /CX flag
#pragma warning(disable: 4447)
#endif

#if defined(COMPONENT_BUILD)
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    switch (reason)
    {
      case DLL_PROCESS_ATTACH:
        {
#if !defined(ANGLE_DISABLE_TRACE)
            FILE *debug = fopen(TRACE_OUTPUT_FILE, "rt");

            if (debug)
            {
                fclose(debug);
                debug = fopen(TRACE_OUTPUT_FILE, "wt");   // Erase

                if (debug)
                {
                    fclose(debug);
                }
            }
#endif

            currentTLS = TlsAlloc();

            if (currentTLS == TLS_OUT_OF_INDEXES)
            {
                return FALSE;
            }
        }
        // Fall throught to initialize index
      case DLL_THREAD_ATTACH:
        {
            egl::Current *current = (egl::Current*)calloc(1, sizeof(egl::Current));

            if (current)
            {
                TlsSetValue(currentTLS, current);

                current->error = EGL_SUCCESS;
                current->API = EGL_OPENGL_ES_API;
                current->display = EGL_NO_DISPLAY;
                current->drawSurface = EGL_NO_SURFACE;
                current->readSurface = EGL_NO_SURFACE;
            }
        }
        break;
      case DLL_THREAD_DETACH:
        {
            void *current = TlsGetValue(currentTLS);

            if (current)
            {
                free(current);
            }
        }
        break;
      case DLL_PROCESS_DETACH:
        {
            void *current = TlsGetValue(currentTLS);

            if (current)
            {
                free(current);
            }

            TlsFree(currentTLS);
        }
        break;
      default:
        break;
    }

    return TRUE;
}
#endif // defined(COMPONENT_BUILD)

namespace egl
{

void setCurrentError(EGLint error)
{
    egl::Current *current = GetCurrent();

    current->error = error;
}

EGLint getCurrentError()
{
    egl::Current *current = GetCurrent();

    return current->error;
}

void setCurrentAPI(EGLenum API)
{
    egl::Current *current = GetCurrent();

    current->API = API;
}

EGLenum getCurrentAPI()
{
    egl::Current *current = GetCurrent();

    return current->API;
}

void setCurrentDisplay(EGLDisplay dpy)
{
    egl::Current *current = GetCurrent();

    current->display = dpy;
}

EGLDisplay getCurrentDisplay()
{
    egl::Current *current = GetCurrent();

    return current->display;
}

void setCurrentDrawSurface(EGLSurface surface)
{
    egl::Current *current = GetCurrent();

    current->drawSurface = surface;
}

EGLSurface getCurrentDrawSurface()
{
    egl::Current *current = GetCurrent();

    return current->drawSurface;
}

void setCurrentReadSurface(EGLSurface surface)
{
    egl::Current *current = GetCurrent();

    current->readSurface = surface;
}

EGLSurface getCurrentReadSurface()
{
    egl::Current *current = GetCurrent();

    return current->readSurface;
}

void error(EGLint errorCode)
{
    egl::setCurrentError(errorCode);
}

}
