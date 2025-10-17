// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_BASE_EXPORT_H_
#define BASE_BASE_EXPORT_H_

// Note that we don't use buildflags to check this before any #includes.
#if !defined(SB_IS_DEFAULT_TC) && defined(STARBOARD) || \
    defined(ENABLE_BUILDFLAG_IS_NATIVE_TARGET_BUILD)
#error Starboard and native target builds should only include copied_base.
#endif

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(BASE_IMPLEMENTATION)
#define BASE_EXPORT __declspec(dllexport)
#else
#define BASE_EXPORT __declspec(dllimport)
#endif  // defined(BASE_IMPLEMENTATION)

#else  // defined(WIN32)
#define BASE_EXPORT __attribute__((visibility("default")))
#endif

#else  // defined(COMPONENT_BUILD)
#define BASE_EXPORT
#endif

#endif  // BASE_BASE_EXPORT_H_
