// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_PUBLIC_C_SYSTEM_SYSTEM_EXPORT_H_
#define CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_PUBLIC_C_SYSTEM_SYSTEM_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(MOJO_LEGACY_SYSTEM_IMPLEMENTATION)
#define MOJO_LEGACY_SYSTEM_EXPORT __declspec(dllexport)
#else
#define MOJO_LEGACY_SYSTEM_EXPORT __declspec(dllimport)
#endif

#else  // !defined(WIN32)

#define MOJO_LEGACY_SYSTEM_EXPORT __attribute__((visibility("default")))

#endif  // defined(WIN32)

#else  // !defined(COMPONENT_BUILD)

#define MOJO_LEGACY_SYSTEM_EXPORT

#endif  // defined(COMPONENT_BUILD)

#endif  // CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_PUBLIC_C_SYSTEM_SYSTEM_EXPORT_H_
