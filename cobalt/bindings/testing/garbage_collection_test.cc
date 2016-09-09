/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/garbage_collection_test_interface.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
typedef BindingsTestBase GarbageCollectionTest;
}  // namespace

TEST_F(GarbageCollectionTest, JSObjectHoldsReferenceToPlatformObject) {
  EXPECT_EQ(GarbageCollectionTestInterface::instances().size(), 0);
  EXPECT_TRUE(
      EvaluateScript("var obj = new GarbageCollectionTestInterface();", NULL));

  // Ensure that this is kept alive after GC is run.
  EXPECT_EQ(GarbageCollectionTestInterface::instances().size(), 1);
  CollectGarbage();
  EXPECT_EQ(GarbageCollectionTestInterface::instances().size(), 1);

  // Ensure that this is destroyed when there are no more references to it from
  // JavaScript.
  EXPECT_TRUE(EvaluateScript("var obj = undefined;", NULL));
  CollectGarbage();
#if !defined(ENGINE_USES_CONSERVATIVE_ROOTING)
  EXPECT_EQ(GarbageCollectionTestInterface::instances().size(), 0);
#endif
}

TEST_F(GarbageCollectionTest, PreventGarbageCollection) {
  EXPECT_EQ(GarbageCollectionTestInterface::instances().size(), 0);
  EXPECT_TRUE(
      EvaluateScript("var obj = new GarbageCollectionTestInterface();", NULL));

  // Keep this instance alive using PreventGarbageCollection
  ASSERT_EQ(GarbageCollectionTestInterface::instances().size(), 1);
  global_object_proxy_->PreventGarbageCollection(
      make_scoped_refptr<script::Wrappable>(
          GarbageCollectionTestInterface::instances()[0]));
  // Remove the only reference to this object from JavaScript.
  EXPECT_TRUE(EvaluateScript("var obj = undefined;", NULL));

  // Ensure that the object is kept alive.
  CollectGarbage();
  ASSERT_EQ(GarbageCollectionTestInterface::instances().size(), 1);

  // Allow this object to be garbage collected once more.
  global_object_proxy_->AllowGarbageCollection(
      make_scoped_refptr<script::Wrappable>(
          GarbageCollectionTestInterface::instances()[0]));

  // Ensure that the object is destroyed by garbage collection.
  CollectGarbage();
#if !defined(ENGINE_USES_CONSERVATIVE_ROOTING)
  EXPECT_EQ(GarbageCollectionTestInterface::instances().size(), 0);
#endif
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt
