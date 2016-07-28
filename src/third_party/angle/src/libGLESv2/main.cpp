#include "precompiled.h"
//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// main.cpp: DLL entry point and management of thread-local data.

#include "libGLESv2/main.h"

#include "libGLESv2/Context.h"

static volatile DWORD currentTLS = TLS_OUT_OF_INDEXES;

static gl::Current* GetCurrent()
{
#if !defined(COMPONENT_BUILD)
    if (currentTLS == TLS_OUT_OF_INDEXES)
        currentTLS = TlsAlloc();
#endif

    gl::Current *current = (gl::Current*)TlsGetValue(currentTLS);

#if !defined(COMPONENT_BUILD)
    if (current == NULL)
    {
        current = (gl::Current*)calloc(1, sizeof(gl::Current));

        if (current)
        {
            TlsSetValue(currentTLS, current);

            current->context = NULL;
            current->display = NULL;
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
            currentTLS = TlsAlloc();

            if (currentTLS == TLS_OUT_OF_INDEXES)
            {
                return FALSE;
            }
        }
        // Fall throught to initialize index
      case DLL_THREAD_ATTACH:
        {
            gl::Current *current = (gl::Current*)calloc(1, sizeof(gl::Current));

            if (current)
            {
                TlsSetValue(currentTLS, current);

                current->context = NULL;
                current->display = NULL;
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

namespace gl
{

void makeCurrent(Context *context, egl::Display *display, egl::Surface *surface)
{
    Current *current = GetCurrent();

    current->context = context;
    current->display = display;

    if (context && display && surface)
    {
        context->makeCurrent(surface);
    }
}

Context *getContext()
{
    Current *current = GetCurrent();

    return current->context;
}

Context *getNonLostContext()
{
    Context *context = getContext();
    
    if (context)
    {
        if (context->isContextLost())
        {
            gl::error(GL_OUT_OF_MEMORY);
            return NULL;
        }
        else
        {
            return context;
        }
    }
    return NULL;
}

egl::Display *getDisplay()
{
    Current *current = GetCurrent();

    return current->display;
}

// Records an error code
void error(GLenum errorCode)
{
    gl::Context *context = glGetCurrentContext();

    if (context)
    {
        switch (errorCode)
        {
          case GL_INVALID_ENUM:
            context->recordInvalidEnum();
            TRACE("\t! Error generated: invalid enum\n");
            break;
          case GL_INVALID_VALUE:
            context->recordInvalidValue();
            TRACE("\t! Error generated: invalid value\n");
            break;
          case GL_INVALID_OPERATION:
            context->recordInvalidOperation();
            TRACE("\t! Error generated: invalid operation\n");
            break;
          case GL_OUT_OF_MEMORY:
            context->recordOutOfMemory();
            TRACE("\t! Error generated: out of memory\n");
            break;
          case GL_INVALID_FRAMEBUFFER_OPERATION:
            context->recordInvalidFramebufferOperation();
            TRACE("\t! Error generated: invalid framebuffer operation\n");
            break;
          default: UNREACHABLE();
        }
    }
}

}

