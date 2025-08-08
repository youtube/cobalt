// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/tests/struct_headers_unittest.test-mojom.h"

// This file contains compile-time assertions about the mojo C++ bindings
// generator to ensure it does not transitively include unnecessary headers.
// It just checks a few common ones - the list doesn't have to be exhaustive.
//
// If this file won't compile, check module.h.tmpl and related .tmpl files.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STRUCT_TRAITS_H_
#error Mojo header guards changed, tests below are invalid.
#endif

#ifdef MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_PTR_H_
#error interface_ptr.h should not be included by the generated header \
    for a mojom containing only a struct.
#endif

#ifdef MOJO_PUBLIC_CPP_BINDINGS_LIB_NATIVE_STRUCT_SERIALIZATION_H_
#error native_struct_serialization.h should not be included by the generated \
    header for a mojom that does not use native structs.
#endif
