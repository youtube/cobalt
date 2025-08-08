// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPL_EXPORT_H_
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPL_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(GLES2_IMPL_IMPLEMENTATION)
#define GLES2_IMPL_EXPORT __declspec(dllexport)
#else
#define GLES2_IMPL_EXPORT __declspec(dllimport)
#endif  // defined(GLES2_IMPL_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(GLES2_IMPL_IMPLEMENTATION)
#define GLES2_IMPL_EXPORT __attribute__((visibility("default")))
#else
#define GLES2_IMPL_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define GLES2_IMPL_EXPORT
#endif

#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPL_EXPORT_H_
