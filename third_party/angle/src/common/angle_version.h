//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// angle_version.h: ANGLE version constants. Generated from git commands.

#ifndef COMMON_ANGLE_VERSION_H_
#define COMMON_ANGLE_VERSION_H_

<<<<<<< HEAD:third_party/angle/src/common/version.h
#if defined(STARBOARD)
// Just use the default commit hash header.
#include "../commit.h"
#else
#include "id/commit.h"
#endif
=======
#include "ANGLEShaderProgramVersion.h"
#include "angle_commit.h"
>>>>>>> 14fc56d09e6b0be117cc05de0d4dbb5a503e54c6:third_party/angle/src/common/angle_version.h

#define ANGLE_MAJOR_VERSION 2
#define ANGLE_MINOR_VERSION 1

#ifndef ANGLE_REVISION
#    define ANGLE_REVISION ANGLE_COMMIT_POSITION
#endif

#define ANGLE_STRINGIFY(x) #x
#define ANGLE_MACRO_STRINGIFY(x) ANGLE_STRINGIFY(x)

#define ANGLE_VERSION_STRING                                                  \
    ANGLE_MACRO_STRINGIFY(ANGLE_MAJOR_VERSION)                                \
    "." ANGLE_MACRO_STRINGIFY(ANGLE_MINOR_VERSION) "." ANGLE_MACRO_STRINGIFY( \
        ANGLE_REVISION) " git hash: " ANGLE_COMMIT_HASH

#endif  // COMMON_ANGLE_VERSION_H_
