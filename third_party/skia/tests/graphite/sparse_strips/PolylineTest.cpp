/*
 * Copyright 2026 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkMatrix.h"
#include "include/core/SkPathBuilder.h"
#include "src/gpu/graphite/sparse_strips/Flatten.h"
#include "src/gpu/graphite/sparse_strips/Polyline.h"
#include "tests/Test.h"

#include <cmath>
#include <iterator>

namespace skgpu::graphite {

DEF_TEST(SparseStrips_Polyline, reporter) {
    // Append Deduplications
    {
        Polyline polyline;
        REPORTER_ASSERT(reporter, polyline.empty());
        REPORTER_ASSERT(reporter, polyline.count() == 0);

        polyline.appendPoint({0.0f, 0.0f});
        REPORTER_ASSERT(reporter, polyline.count() == 1);

        polyline.appendPoint({0.0f, 0.0f});
        REPORTER_ASSERT(reporter, polyline.count() == 1);

        polyline.appendPoint({1.0f, 1.0f});
        REPORTER_ASSERT(reporter, polyline.count() == 2);

        polyline.appendSentinel();
        REPORTER_ASSERT(reporter, polyline.count() == 3);
        REPORTER_ASSERT(reporter, std::isnan(polyline.points().back().fX));
        REPORTER_ASSERT(reporter, std::isnan(polyline.points().back().fY));

        polyline.appendSentinel();
        REPORTER_ASSERT(reporter, polyline.count() == 3);

        polyline.reset();
        REPORTER_ASSERT(reporter, polyline.empty());
        polyline.appendSentinel();
        REPORTER_ASSERT(reporter, polyline.empty());
    }

    // Iterator
    {
        Polyline polyline;

        polyline.appendPoint({0.0f, 0.0f});
        polyline.appendPoint({1.0f, 0.0f});
        polyline.appendPoint({1.0f, 1.0f});
        polyline.appendSentinel();

        polyline.appendPoint({5.0f, 5.0f});
        polyline.appendPoint({6.0f, 6.0f});
        polyline.appendSentinel();

        polyline.appendPoint({10.0f, 10.0f});
        polyline.appendSentinel();

        int expectedIndices[] = {0, 1, 4};
        int count = 0;

        for (auto it = polyline.begin(); it != polyline.end(); ++it) {
            int idx = (*it).second;
            REPORTER_ASSERT(reporter, count < 3);
            REPORTER_ASSERT(reporter, idx == expectedIndices[count]);
            count++;
        }

        REPORTER_ASSERT(reporter, count == 3);
    }

    // Malformed inputs
    {
        static constexpr float kNaN = SK_ScalarNaN;
        static constexpr SkPoint kPathologicalPts[] = {
            {kNaN, kNaN},
            {kNaN, kNaN},
            {kNaN, kNaN},
            {kNaN, kNaN},
            {kNaN, kNaN},
            {0.0f, 0.0f},
            {kNaN, kNaN},
            {kNaN, kNaN},
            {1.0f, 1.0f},
            {2.0f, 2.0f},
            {kNaN, kNaN},
            {kNaN, kNaN},
            {kNaN, kNaN},
            {kNaN, kNaN},
            {kNaN, kNaN},
            {kNaN, kNaN},
            {3.0f, 3.0f},
            {kNaN, kNaN},
            {4.0f, 4.0f},
            {5.0f, 5.0f},
            {6.0f, 6.0f},
            {kNaN, kNaN},
            {7.0f, 7.0f},
            {kNaN, kNaN},
            {kNaN, kNaN},
            {kNaN, kNaN},
            {kNaN, kNaN}
        };

        int count = std::size(kPathologicalPts);
        Polyline::LineIterator it(kPathologicalPts, 0, count);
        Polyline::LineIterator end(kPathologicalPts, count, count);

        REPORTER_ASSERT(reporter, it != end);
        REPORTER_ASSERT(reporter, (*it).second == 8);
        ++it;

        REPORTER_ASSERT(reporter, it != end);
        REPORTER_ASSERT(reporter, (*it).second == 18);
        ++it;

        REPORTER_ASSERT(reporter, it != end);
        REPORTER_ASSERT(reporter, (*it).second == 19);
        ++it;

        REPORTER_ASSERT(reporter, !(it != end));
    }

    // Empty/Null
    {
        Polyline polyline;

        auto it = polyline.begin();
        auto end = polyline.end();

        REPORTER_ASSERT(reporter, !(it != end));

        const SkPoint test[] = {{0.0f, 0.0f}};
        Polyline::LineIterator rawIt(test, 0, 0);
        Polyline::LineIterator rawEnd(test, 0, 0);

        REPORTER_ASSERT(reporter, !(rawIt != rawEnd));
    }

    // Single Point
    {
        const SkPoint pts[] = {{1.0f, 1.0f}};
        Polyline::LineIterator it(pts, 0, 1);
        Polyline::LineIterator end(pts, 1, 1);

        REPORTER_ASSERT(reporter, !(it != end));
    }
}

DEF_TEST(SparseStrips_Polyline_Integrated, reporter) {
    // Simple rect
    {
        SkPathBuilder builder;
        builder.addRect(SkRect::MakeXYWH(10, 10, 50, 50));
        SkPath path = builder.detach();

        Flatten flatten;
        Polyline polyline;

        flatten.processPaths<FlattenMode::kScalar>(path, SkMatrix::I(), 100.0f, 100.0f, &polyline);

        REPORTER_ASSERT(reporter, polyline.count() == 6);
        const auto& pts = polyline.points();
        REPORTER_ASSERT(reporter, pts[0] == SkPoint::Make(10, 10));
        REPORTER_ASSERT(reporter, pts[4] == SkPoint::Make(10, 10));
        REPORTER_ASSERT(reporter, std::isnan(pts[5].fX));

        int lineCount = 0;
        for (auto [line, index] : polyline) {
            lineCount++;
            REPORTER_ASSERT(reporter, !std::isnan(line.p0.fX) && !std::isnan(line.p1.fX));
        }
        REPORTER_ASSERT(reporter, lineCount == 4);
    }

    // NaN injection between sub-paths
    {
        SkPathBuilder builder;
        builder.moveTo(0, 0);
        builder.lineTo(10, 0);

        builder.moveTo(20, 20); // Triggers sentinel for previous open path
        builder.lineTo(30, 20);
        SkPath path = builder.detach();

        Flatten flatten;
        Polyline polyline;

        flatten.processPaths<FlattenMode::kScalar>(path, SkMatrix::I(), 100.0f, 100.0f, &polyline);

        REPORTER_ASSERT(reporter, polyline.count() == 8);

        const auto& pts = polyline.points();
        REPORTER_ASSERT(reporter, pts[1] == SkPoint::Make(10, 0));
        REPORTER_ASSERT(reporter, pts[2] == SkPoint::Make(0, 0));
        REPORTER_ASSERT(reporter, std::isnan(pts[3].fX));

        REPORTER_ASSERT(reporter, pts[4] == SkPoint::Make(20, 20));
        REPORTER_ASSERT(reporter, std::isnan(pts[7].fX));

        int lineCount = 0;
        for (auto [line, index] : polyline) {
            lineCount++;
            REPORTER_ASSERT(reporter, index != 2 && index != 3 && index != 6 && index != 7);
        }
        REPORTER_ASSERT(reporter, lineCount == 4);
    }

    // Deduplication
    {
        SkPathBuilder builder;
        builder.moveTo(0, 0);
        builder.lineTo(10, 10);
        builder.lineTo(10, 10);
        builder.lineTo(20, 0);
        builder.close();
        SkPath path = builder.detach();

        Flatten flatten;
        Polyline polyline;

        flatten.processPaths<FlattenMode::kScalar>(path, SkMatrix::I(), 100.0f, 100.0f, &polyline);

        REPORTER_ASSERT(reporter, polyline.count() == 5);
        const auto& pts = polyline.points();
        REPORTER_ASSERT(reporter, pts[1] == SkPoint::Make(10, 10));
        REPORTER_ASSERT(reporter, pts[2] == SkPoint::Make(20, 0));
    }

    // Simplifying a quad to a line due to culling
    {
        SkPathBuilder builder;
        builder.moveTo(150, 150);
        builder.quadTo(160, 160, 170, 150);
        SkPath path = builder.detach();

        Flatten flatten;
        Polyline polyline;

        flatten.processPaths<FlattenMode::kScalar>(path, SkMatrix::I(), 100.0f, 100.0f, &polyline);

        REPORTER_ASSERT(reporter, polyline.count() == 4);
        const auto& pts = polyline.points();
        REPORTER_ASSERT(reporter, pts[0] == SkPoint::Make(150, 150));
        REPORTER_ASSERT(reporter, pts[1] == SkPoint::Make(170, 150));
        REPORTER_ASSERT(reporter, pts[2] == SkPoint::Make(150, 150));
        REPORTER_ASSERT(reporter, std::isnan(pts[3].fX));
    }

    // Simplifying cubic to a line due to culling
    {
        SkPathBuilder builder;
        builder.moveTo(-10, -10);
        builder.cubicTo(-20, -20, -30, -20, -40, -10);
        SkPath path = builder.detach();

        Flatten flatten;
        Polyline polyline;

        flatten.processPaths<FlattenMode::kScalar>(path, SkMatrix::I(), 100.0f, 100.0f, &polyline);

        REPORTER_ASSERT(reporter, polyline.count() == 4);
        const auto& pts = polyline.points();
        REPORTER_ASSERT(reporter, pts[0] == SkPoint::Make(-10, -10));
        REPORTER_ASSERT(reporter, pts[1] == SkPoint::Make(-40, -10));
        REPORTER_ASSERT(reporter, pts[2] == SkPoint::Make(-10, -10));
        REPORTER_ASSERT(reporter, std::isnan(pts[3].fX));
    }
}

}  // namespace skgpu::graphite
