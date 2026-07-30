// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_SYSTEM_IMPL_EXPORT_H_
#define CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_SYSTEM_IMPL_EXPORT_H_

#if defined(MOJO_LEGACY_CORE_SHARED_LIBRARY)
#define MOJO_LEGACY_SYSTEM_IMPL_EXPORT
#else
#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(MOJO_LEGACY_SYSTEM_IMPL_IMPLEMENTATION)
#define MOJO_LEGACY_SYSTEM_IMPL_EXPORT __declspec(dllexport)
#else
#define MOJO_LEGACY_SYSTEM_IMPL_EXPORT __declspec(dllimport)
#endif  // defined(MOJO_LEGACY_SYSTEM_IMPL_IMPLEMENTATION)

#else  // defined(WIN32)
#define MOJO_LEGACY_SYSTEM_IMPL_EXPORT __attribute__((visibility("default")))
#endif

#else  // defined(COMPONENT_BUILD)
#define MOJO_LEGACY_SYSTEM_IMPL_EXPORT
#endif
#endif

#endif  // CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_SYSTEM_IMPL_EXPORT_H_
