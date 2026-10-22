// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/starboard_wasm/guest_memory.h"

#include "gin/converter.h"
#include "gin/test/v8_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-script.h"
#include "v8/include/v8-wasm.h"

namespace cobalt {
namespace starboard_wasm {
namespace {

constexpr uint32_t kPageSize = 65536;

class GuestMemoryTest : public gin::V8Test {
 protected:
  void SetUp() override {
    gin::V8Test::SetUp();
    v8::Isolate* isolate = instance_->isolate();
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::Value> memory =
        v8::Script::Compile(
            context,
            gin::StringToV8(isolate, "new WebAssembly.Memory({initial: 1})"))
            .ToLocalChecked()
            ->Run(context)
            .ToLocalChecked();
    ASSERT_TRUE(memory->IsWasmMemoryObject());
    memory_.Reset(isolate, memory.As<v8::WasmMemoryObject>());
  }

  void TearDown() override {
    memory_.Reset();
    gin::V8Test::TearDown();
  }

  v8::Global<v8::WasmMemoryObject> memory_;
};

TEST_F(GuestMemoryTest, ResolvesRangesInsideMemory) {
  GuestMemory memory(instance_->isolate(), memory_);
  uint8_t* base = memory.Bytes(0, 1);
  ASSERT_TRUE(base);
  EXPECT_EQ(memory.Bytes(kPageSize - 4, 4), base + kPageSize - 4);
  EXPECT_EQ(memory.Array(8, 4, 16), base + 8);
}

TEST_F(GuestMemoryTest, RejectsEmptyAndOutOfBoundsRanges) {
  GuestMemory memory(instance_->isolate(), memory_);
  EXPECT_FALSE(memory.Bytes(0, 0));
  EXPECT_FALSE(memory.Bytes(kPageSize - 4, 5));
  EXPECT_FALSE(memory.Bytes(kPageSize, 1));
  EXPECT_FALSE(memory.Bytes(0xffffffff, 2));
  EXPECT_FALSE(memory.Array(0, kPageSize / 16 + 1, 16));
  EXPECT_FALSE(memory.Array(16, 0x80000000, 4));
}

TEST_F(GuestMemoryTest, FailsWithoutMemory) {
  v8::Global<v8::WasmMemoryObject> none;
  GuestMemory memory(instance_->isolate(), none);
  EXPECT_FALSE(memory.Bytes(0, 1));
}

}  // namespace
}  // namespace starboard_wasm
}  // namespace cobalt
