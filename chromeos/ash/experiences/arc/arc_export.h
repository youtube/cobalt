// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_ARC_EXPORT_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_ARC_EXPORT_H_

#include "build/build_config.h"

static_assert(BUILDFLAG(IS_CHROMEOS), "ARC can be built only for ChromeOS.");

#if defined(COMPONENT_BUILD) && defined(ARC_IMPLEMENTATION)
#define ARC_EXPORT __attribute__((visibility("default")))
#else  // !defined(COMPONENT_BUILD) || !defined(ARC_IMPLEMENTATION)
#define ARC_EXPORT
#endif

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_ARC_EXPORT_H_
