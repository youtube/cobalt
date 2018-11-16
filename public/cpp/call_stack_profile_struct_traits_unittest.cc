// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "components/metrics/public/interfaces/call_stack_profile_collector_test.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

class CallStackProfileCollectorTestImpl
    : public mojom::CallStackProfileCollectorTest {
 public:
  explicit CallStackProfileCollectorTestImpl(
      mojo::InterfaceRequest<mojom::CallStackProfileCollectorTest> request)
      : binding_(this, std::move(request)) {
  }

  // CallStackProfileCollectorTest:
  void BounceSampledProfile(SampledProfile in,
                            BounceSampledProfileCallback callback) override {
    std::move(callback).Run(in);
  }

 private:
  mojo::Binding<CallStackProfileCollectorTest> binding_;

  DISALLOW_COPY_AND_ASSIGN(CallStackProfileCollectorTestImpl);
};

class CallStackProfileStructTraitsTest : public testing::Test {
 public:
  CallStackProfileStructTraitsTest() : impl_(MakeRequest(&proxy_)) {}

 protected:
  base::MessageLoop message_loop_;
  mojom::CallStackProfileCollectorTestPtr proxy_;
  CallStackProfileCollectorTestImpl impl_;

  DISALLOW_COPY_AND_ASSIGN(CallStackProfileStructTraitsTest);
};

// Checks serialization/deserialization of SampledProfile.
TEST_F(CallStackProfileStructTraitsTest, SampledProfile) {
  // Construct a SampledProfile protocol buffer message.
  SampledProfile input_proto;

  CallStackProfile* proto_profile = input_proto.mutable_call_stack_profile();

  CallStackProfile::Sample* proto_sample =
      proto_profile->add_deprecated_sample();
  proto_sample->set_count(1);
  CallStackProfile::Location* location = proto_sample->add_frame();
  location->set_address(0x10ULL);
  location->set_module_id_index(0);

  CallStackProfile::ModuleIdentifier* module_id =
      proto_profile->add_module_id();
  module_id->set_build_id("a");
  module_id->set_name_md5_prefix(111U);

  proto_profile->set_profile_duration_ms(1000);
  proto_profile->set_sampling_period_ms(2000);

  // Send the message round trip, and verify those values.
  SampledProfile output_proto;
  EXPECT_TRUE(
      proxy_->BounceSampledProfile(std::move(input_proto), &output_proto));

  const CallStackProfile& out_profile = output_proto.call_stack_profile();

  ASSERT_EQ(1, out_profile.deprecated_sample_size());
  ASSERT_EQ(1, out_profile.deprecated_sample(0).frame_size());

  ASSERT_TRUE(out_profile.deprecated_sample(0).frame(0).has_address());
  EXPECT_EQ(0x10ULL, out_profile.deprecated_sample(0).frame(0).address());

  ASSERT_TRUE(out_profile.deprecated_sample(0).frame(0).has_module_id_index());
  EXPECT_EQ(0, out_profile.deprecated_sample(0).frame(0).module_id_index());

  ASSERT_EQ(1, out_profile.module_id().size());

  ASSERT_TRUE(out_profile.module_id(0).has_build_id());
  ASSERT_EQ("a", out_profile.module_id(0).build_id());

  ASSERT_TRUE(out_profile.module_id(0).has_name_md5_prefix());
  ASSERT_EQ(111U, out_profile.module_id(0).name_md5_prefix());

  ASSERT_TRUE(out_profile.has_profile_duration_ms());
  EXPECT_EQ(1000, out_profile.profile_duration_ms());

  ASSERT_TRUE(out_profile.has_sampling_period_ms());
  EXPECT_EQ(2000, out_profile.sampling_period_ms());
}

}  // namespace metrics
