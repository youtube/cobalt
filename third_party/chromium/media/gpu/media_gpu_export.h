// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_GPU_MEDIA_GPU_EXPORT_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_GPU_MEDIA_GPU_EXPORT_H_

// Define MEDIA_GPU_EXPORT so that functionality implemented by the Media GPU
// module can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(MEDIA_GPU_IMPLEMENTATION)
#define THIRD_PARTY_CHROMIUM_MEDIA_GPU_EXPORT __declspec(dllexport)
#else
#define THIRD_PARTY_CHROMIUM_MEDIA_GPU_EXPORT __declspec(dllimport)
#endif  // defined(MEDIA_GPU_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(MEDIA_GPU_IMPLEMENTATION)
#define THIRD_PARTY_CHROMIUM_MEDIA_GPU_EXPORT __attribute__((visibility("default")))
#else
#define THIRD_PARTY_CHROMIUM_MEDIA_GPU_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define THIRD_PARTY_CHROMIUM_MEDIA_GPU_EXPORT
#endif

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_GPU_MEDIA_GPU_EXPORT_H_
