// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/embedder/graph_features.h"

#include "build/build_config.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/v8_memory/v8_context_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

TEST(GraphFeaturesTest, ConfigureGraph) {
  GraphFeatures features;

  EXPECT_FALSE(features.flags().execution_context_registry);
  EXPECT_FALSE(features.flags().v8_context_tracker);
  features.EnableV8ContextTracker();
  EXPECT_TRUE(features.flags().execution_context_registry);
  EXPECT_TRUE(features.flags().v8_context_tracker);

  TestGraphImpl graph;
  graph.SetUp();

  EXPECT_FALSE(v8_memory::V8ContextTracker::GetFromGraph(&graph));
  features.ConfigureGraph(&graph);
  EXPECT_TRUE(v8_memory::V8ContextTracker::GetFromGraph(&graph));

  graph.TearDown();
}

TEST(GraphFeaturesTest, EnableDefault) {
  GraphFeatures features;
  TestGraphImpl graph;
  graph.SetUp();

  EXPECT_EQ(0u, graph.GraphOwnedCountForTesting());
  EXPECT_EQ(0u, graph.GraphRegisteredCountForTesting());
  EXPECT_EQ(0u, graph.NodeDataDescriberCountForTesting());
  EXPECT_FALSE(
      execution_context::ExecutionContextRegistry::GetFromGraph(&graph));
  EXPECT_FALSE(v8_memory::V8ContextTracker::GetFromGraph(&graph));

  // An empty config should install nothing.
  features.ConfigureGraph(&graph);
  EXPECT_EQ(0u, graph.GraphOwnedCountForTesting());
  EXPECT_EQ(0u, graph.GraphRegisteredCountForTesting());
  EXPECT_EQ(0u, graph.NodeDataDescriberCountForTesting());
  EXPECT_FALSE(
      execution_context::ExecutionContextRegistry::GetFromGraph(&graph));
  EXPECT_FALSE(v8_memory::V8ContextTracker::GetFromGraph(&graph));

  size_t graph_owned_count = 11;
#if !BUILDFLAG(IS_ANDROID)
  // The SiteDataRecorder is not available on Android.
  graph_owned_count++;
#endif

  // Validate that the default configuration works as expected.
  features.EnableDefault();
  features.ConfigureGraph(&graph);
  EXPECT_EQ(graph_owned_count, graph.GraphOwnedCountForTesting());
  EXPECT_EQ(3u, graph.GraphRegisteredCountForTesting());
  EXPECT_EQ(7u, graph.NodeDataDescriberCountForTesting());
  // Ensure the GraphRegistered objects can be queried directly.
  EXPECT_TRUE(
      execution_context::ExecutionContextRegistry::GetFromGraph(&graph));
  EXPECT_TRUE(v8_memory::V8ContextTracker::GetFromGraph(&graph));

  graph.TearDown();
}

TEST(GraphFeaturesTest, StandardConfigurations) {
  GraphFeatures features;
  EXPECT_EQ(features.flags().flags, GraphFeatures::WithNone().flags().flags);
  features.EnableMinimal();
  EXPECT_EQ(features.flags().flags, GraphFeatures::WithMinimal().flags().flags);

  // This test will fail if Default is not a superset of Minimal, since it does
  // not remove the Minimal flags. That's a good thing to test since it would
  // be confusing for Minimal to include features that Default doesn't.
  features.EnableDefault();
  EXPECT_EQ(features.flags().flags, GraphFeatures::WithDefault().flags().flags);
}

}  // namespace performance_manager
