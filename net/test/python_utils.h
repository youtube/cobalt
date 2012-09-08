// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_PYTHON_UTILS_H_
#define NET_TEST_PYTHON_UTILS_H_

#include "base/compiler_specific.h"

class FilePath;

// This is the python path variable name.
extern const char kPythonPathEnv[];

// This is the python unbuffered variable name.
extern const char kPythonUnbufferedEnv[];

// Appends the dir to python path environment variable.
void AppendToPythonPath(const FilePath& dir);

// Return the location of the compiler-generated python protobuf.
bool GetPyProtoPath(FilePath* dir);

// Returns the path that should be used to launch Python.
bool GetPythonRunTime(FilePath* path) WARN_UNUSED_RESULT;

// Sets the environment variable that forces python console output to be
// unbuffered. Used so that buffered logs don't interfere with gtest logs after
// a test has been run.
void EnablePythonUnbufferedMode();

#endif  // NET_TEST_PYTHON_UTILS_H_
