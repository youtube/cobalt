// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_WIN_WIN_HANDLE_TYPES_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_WIN_WIN_HANDLE_TYPES_H_

// Forward declare Windows compatible handles.

#define PA_WINDOWS_HANDLE_TYPE(name) \
  struct name##__;                   \
  typedef struct name##__* name;
#include "base/allocator/partition_allocator/partition_alloc_base/win/win_handle_types_list.inc"
#undef PA_WINDOWS_HANDLE_TYPE

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_WIN_WIN_HANDLE_TYPES_H_
