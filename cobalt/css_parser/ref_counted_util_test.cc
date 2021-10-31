// Copyright 2014 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/css_parser/ref_counted_util.h"

#include "gtest/gtest.h"

namespace cobalt {
namespace css_parser {

class RefCountedDummy : public base::RefCounted<RefCountedDummy> {};

TEST(RefCountedUtilTest, AddRefToNull) {
  RefCountedDummy* dummy_ptr = NULL;
  ASSERT_FALSE(AddRef(dummy_ptr));
}

TEST(RefCountedUtilTest, AddRefToNonNull) {
  RefCountedDummy* dummy_ptr = new RefCountedDummy();
  ASSERT_EQ(dummy_ptr, AddRef(dummy_ptr));
  EXPECT_TRUE(dummy_ptr->HasOneRef());
  dummy_ptr->Release();
}

TEST(RefCountedUtilTest, MakesScopedRefPtrFromNull) {
  RefCountedDummy* dummy_ptr = NULL;

  scoped_refptr<RefCountedDummy> dummy_refptr =
      MakeScopedRefPtrAndRelease(dummy_ptr);
  ASSERT_FALSE(dummy_refptr.get());
}

TEST(RefCountedUtilTest, MakesScopedRefPtrFromNonNull) {
  RefCountedDummy* dummy_ptr = AddRef(new RefCountedDummy());

  scoped_refptr<RefCountedDummy> dummy_refptr =
      MakeScopedRefPtrAndRelease(dummy_ptr);
  ASSERT_EQ(dummy_ptr, dummy_refptr.get());
  EXPECT_TRUE(dummy_refptr->HasOneRef());
}

}  // namespace css_parser
}  // namespace cobalt
