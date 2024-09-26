// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_WEBGPU_EXPORT_H_
#define GPU_COMMAND_BUFFER_CLIENT_WEBGPU_EXPORT_H_

#if defined(COMPONENT_BUILD) && !defined(NACL_WIN64)
#if defined(WIN32)

#if defined(WEBGPU_IMPLEMENTATION)
#define WEBGPU_EXPORT __declspec(dllexport)
#else
#define WEBGPU_EXPORT __declspec(dllimport)
#endif  // defined(WEBGPU_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(WEBGPU_IMPLEMENTATION)
#define WEBGPU_EXPORT __attribute__((visibility("default")))
#else
#define WEBGPU_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define WEBGPU_EXPORT
#endif

#endif  // GPU_COMMAND_BUFFER_CLIENT_WEBGPU_EXPORT_H_
