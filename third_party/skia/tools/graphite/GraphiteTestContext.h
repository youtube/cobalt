/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skiatest_graphite_GraphiteTestContext_DEFINED
#define skiatest_graphite_GraphiteTestContext_DEFINED

#include "experimental/graphite/include/GraphiteTypes.h"
#include "include/core/SkRefCnt.h"

namespace skgpu { class Context; }

namespace skiatest::graphite {

/**
 * An offscreen 3D context. This class is intended for Skia's internal testing needs and not
 * for general use.
 */
class GraphiteTestContext {
public:
    GraphiteTestContext(const GraphiteTestContext&) = delete;
    GraphiteTestContext& operator=(const GraphiteTestContext&) = delete;

    virtual ~GraphiteTestContext();

    virtual skgpu::BackendApi backend() = 0;

    virtual std::unique_ptr<skgpu::Context> makeContext() = 0;

protected:
    GraphiteTestContext();
};


}  // namespace skiatest::graphite

#endif // skiatest_graphite_GraphiteTestContext_DEFINED
