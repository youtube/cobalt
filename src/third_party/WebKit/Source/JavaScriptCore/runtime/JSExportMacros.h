/*
 * Copyright (C) 2011 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 *
 * This file handles shared library symbol export decorations. It is recommended
 * that all WebKit projects use these definitions so that symbol exports work
 * properly on all platforms and compilers that WebKit builds under.
 */

#ifndef JSExportMacros_h
#define JSExportMacros_h

#include <wtf/Platform.h>
#include <wtf/ExportMacros.h>

// See note in wtf/Platform.h for more info on EXPORT_MACROS.
#if USE(EXPORT_MACROS)

#if defined(BUILDING_JavaScriptCore) || defined(STATICALLY_LINKED_WITH_JavaScriptCore)
#define JS_EXPORT_PRIVATE WTF_EXPORT
#else
#define JS_EXPORT_PRIVATE WTF_IMPORT
#endif

#define JS_EXPORT_HIDDEN WTF_HIDDEN
// NOTE: Cobalt doesn't support exporting data on all platforms.
#define JS_EXPORTDATA
#define JS_EXPORTCLASS JS_EXPORT_PRIVATE

#else // !USE(EXPORT_MACROS)

#if !PLATFORM(CHROMIUM) && OS(WINDOWS) && !defined(BUILDING_WX__) && !COMPILER(GCC)

#if defined(BUILDING_JavaScriptCore) || defined(STATICALLY_LINKED_WITH_JavaScriptCore)
#define JS_EXPORTDATA __declspec(dllexport)
#else
#define JS_EXPORTDATA __declspec(dllimport)
#endif

#define JS_EXPORTCLASS JS_EXPORTDATA

#else // !PLATFORM...

#define JS_EXPORTDATA
#define JS_EXPORTCLASS

#endif // !PLATFORM...

#define JS_EXPORT_PRIVATE
#define JS_EXPORT_HIDDEN

#endif // USE(EXPORT_MACROS)

// Stock JavaScriptCore expects the ClassInfo struct to be named s_info
// and have public accessibility. However to avoid importing s_info structs
// across DLL boundaries, we make s_info private and use s_classinfo()
// This macro must be placed in the 'public' section of the class declaration.

#define DECLARE_CLASSINFO() \
private: \
    static const JSC::ClassInfo s_info; \
public: \
    static const JSC::ClassInfo* s_classinfo() { return &s_info; }

#define DECLARE_EXPORTED_CLASSINFO() \
private: \
    static const JSC::ClassInfo s_info; \
public: \
    static JS_EXPORT_PRIVATE const JSC::ClassInfo* s_classinfo()

#endif // JSExportMacros_h
