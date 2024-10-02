// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/tests/nullable_numerics_and_enums_unittest.test-mojom.h"
#include "mojo/public/cpp/bindings/tests/nullable_numerics_and_enums_unittest_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo::test::nullable_numerics_and_enums_unittest {
namespace {

template <typename T>
inline constexpr T kDefaultValue = T();

template <>
inline constexpr mojom::RegularEnum kDefaultValue<mojom::RegularEnum> =
    mojom::RegularEnum::kThisValue;

template <>
inline constexpr TypemappedEnum kDefaultValue<TypemappedEnum> =
    TypemappedEnum::kValueOne;

template <typename T>
std::pair<bool, T> FromOpt(const absl::optional<T>& opt) {
  return opt.has_value() ? std::pair(true, opt.value())
                         : std::pair(false, kDefaultValue<T>);
}

template <typename T>
absl::optional<T> ToOpt(bool has_value, T value) {
  return has_value ? absl::make_optional(value) : absl::nullopt;
}

absl::optional<mojom::RegularEnum> Transform(
    absl::optional<mojom::RegularEnum> in) {
  if (!in.has_value()) {
    return absl::nullopt;
  }
  switch (in.value()) {
    case mojom::RegularEnum::kThisValue:
      return mojom::RegularEnum::kThatValue;
    case mojom::RegularEnum::kThatValue:
      return mojom::RegularEnum::kThisValue;
  }
}

absl::optional<TypemappedEnum> Transform(absl::optional<TypemappedEnum> in) {
  if (!in.has_value()) {
    return absl::nullopt;
  }
  switch (in.value()) {
    case TypemappedEnum::kValueOne:
      return TypemappedEnum::kValueTwo;
    case TypemappedEnum::kValueTwo:
      return TypemappedEnum::kValueOne;
  }
}

absl::optional<bool> Transform(absl::optional<bool> in) {
  if (!in.has_value()) {
    return absl::nullopt;
  }
  return !in.value();
}

absl::optional<uint8_t> Transform(absl::optional<uint8_t> in) {
  if (!in.has_value()) {
    return absl::nullopt;
  }
  return ~in.value();
}

absl::optional<uint16_t> Transform(absl::optional<uint16_t> in) {
  if (!in.has_value()) {
    return absl::nullopt;
  }
  return ~in.value();
}

absl::optional<uint32_t> Transform(absl::optional<uint32_t> in) {
  if (!in.has_value()) {
    return absl::nullopt;
  }
  return ~in.value();
}

absl::optional<uint64_t> Transform(absl::optional<uint64_t> in) {
  if (!in.has_value()) {
    return absl::nullopt;
  }
  return ~in.value();
}

absl::optional<int8_t> Transform(absl::optional<int8_t> in) {
  if (!in.has_value()) {
    return absl::nullopt;
  }
  return -in.value();
}

absl::optional<int16_t> Transform(absl::optional<int16_t> in) {
  if (!in.has_value()) {
    return absl::nullopt;
  }
  return -in.value();
}

absl::optional<int32_t> Transform(absl::optional<int32_t> in) {
  if (!in.has_value()) {
    return absl::nullopt;
  }
  return -in.value();
}

absl::optional<int64_t> Transform(absl::optional<int64_t> in) {
  if (!in.has_value()) {
    return absl::nullopt;
  }
  return -in.value();
}

absl::optional<float> Transform(absl::optional<float> in) {
  if (!in.has_value()) {
    return absl::nullopt;
  }
  return -2 * in.value();
}

absl::optional<double> Transform(absl::optional<double> in) {
  if (!in.has_value()) {
    return absl::nullopt;
  }
  return -2 * in.value();
}

class InterfaceV1Impl : public mojom::InterfaceV1 {
 public:
  explicit InterfaceV1Impl(PendingReceiver<mojom::InterfaceV1> receiver)
      : receiver_(this, std::move(receiver)) {}

 private:
  // mojom::InterfaceV1 implementation:
  void MethodWithEnums(bool has_enum_value,
                       mojom::RegularEnum enum_value,
                       bool has_mapped_enum_value,
                       TypemappedEnum mapped_enum_value,
                       MethodWithEnumsCallback callback) override {
    auto [out_has_enum_value, out_enum_value] =
        FromOpt(Transform(ToOpt(has_enum_value, enum_value)));
    auto [out_has_mapped_enum_value, out_mapped_enum_value] =
        FromOpt(Transform(ToOpt(has_mapped_enum_value, mapped_enum_value)));
    std::move(callback).Run(out_has_enum_value, out_enum_value,
                            out_has_mapped_enum_value, out_mapped_enum_value);
  }

  void MethodWithStructWithEnums(
      mojom::CompatibleStructWithEnumsPtr in,
      MethodWithStructWithEnumsCallback callback) override {
    auto [out_has_enum_value, out_enum_value] =
        FromOpt(Transform(ToOpt(in->has_enum_value, in->enum_value)));
    auto [out_has_mapped_enum_value, out_mapped_enum_value] = FromOpt(
        Transform(ToOpt(in->has_mapped_enum_value, in->mapped_enum_value)));
    std::move(callback).Run(mojom::CompatibleStructWithEnums::New(
        out_has_enum_value, out_enum_value, out_has_mapped_enum_value,
        out_mapped_enum_value));
  }

  void MethodWithNumerics(bool has_bool_value,
                          bool bool_value,
                          bool has_u8_value,
                          uint8_t u8_value,
                          bool has_u16_value,
                          uint16_t u16_value,
                          bool has_u32_value,
                          uint32_t u32_value,
                          bool has_u64_value,
                          uint64_t u64_value,
                          bool has_i8_value,
                          int8_t i8_value,
                          bool has_i16_value,
                          int16_t i16_value,
                          bool has_i32_value,
                          int32_t i32_value,
                          bool has_i64_value,
                          int64_t i64_value,
                          bool has_float_value,
                          float float_value,
                          bool has_double_value,
                          double double_value,
                          MethodWithNumericsCallback callback) override {
    auto [out_has_bool_value, out_bool_value] =
        FromOpt(Transform(ToOpt(has_bool_value, bool_value)));
    auto [out_has_u8_value, out_u8_value] =
        FromOpt(Transform(ToOpt(has_u8_value, u8_value)));
    auto [out_has_u16_value, out_u16_value] =
        FromOpt(Transform(ToOpt(has_u16_value, u16_value)));
    auto [out_has_u32_value, out_u32_value] =
        FromOpt(Transform(ToOpt(has_u32_value, u32_value)));
    auto [out_has_u64_value, out_u64_value] =
        FromOpt(Transform(ToOpt(has_u64_value, u64_value)));
    auto [out_has_i8_value, out_i8_value] =
        FromOpt(Transform(ToOpt(has_i8_value, i8_value)));
    auto [out_has_i16_value, out_i16_value] =
        FromOpt(Transform(ToOpt(has_i16_value, i16_value)));
    auto [out_has_i32_value, out_i32_value] =
        FromOpt(Transform(ToOpt(has_i32_value, i32_value)));
    auto [out_has_i64_value, out_i64_value] =
        FromOpt(Transform(ToOpt(has_i64_value, i64_value)));
    auto [out_has_float_value, out_float_value] =
        FromOpt(Transform(ToOpt(has_float_value, float_value)));
    auto [out_has_double_value, out_double_value] =
        FromOpt(Transform(ToOpt(has_double_value, double_value)));
    std::move(callback).Run(
        out_has_bool_value, out_bool_value, out_has_u8_value, out_u8_value,
        out_has_u16_value, out_u16_value, out_has_u32_value, out_u32_value,
        out_has_u64_value, out_u64_value, out_has_i8_value, out_i8_value,
        out_has_i16_value, out_i16_value, out_has_i32_value, out_i32_value,
        out_has_i64_value, out_i64_value, out_has_float_value, out_float_value,
        out_has_double_value, out_double_value);
  }

  void MethodWithStructWithNumerics(
      mojom::CompatibleStructWithNumericsPtr in,
      MethodWithStructWithNumericsCallback callback) override {
    auto [out_has_bool_value, out_bool_value] =
        FromOpt(Transform(ToOpt(in->has_bool_value, in->bool_value)));
    auto [out_has_u8_value, out_u8_value] =
        FromOpt(Transform(ToOpt(in->has_u8_value, in->u8_value)));
    auto [out_has_u16_value, out_u16_value] =
        FromOpt(Transform(ToOpt(in->has_u16_value, in->u16_value)));
    auto [out_has_u32_value, out_u32_value] =
        FromOpt(Transform(ToOpt(in->has_u32_value, in->u32_value)));
    auto [out_has_u64_value, out_u64_value] =
        FromOpt(Transform(ToOpt(in->has_u64_value, in->u64_value)));
    auto [out_has_i8_value, out_i8_value] =
        FromOpt(Transform(ToOpt(in->has_i8_value, in->i8_value)));
    auto [out_has_i16_value, out_i16_value] =
        FromOpt(Transform(ToOpt(in->has_i16_value, in->i16_value)));
    auto [out_has_i32_value, out_i32_value] =
        FromOpt(Transform(ToOpt(in->has_i32_value, in->i32_value)));
    auto [out_has_i64_value, out_i64_value] =
        FromOpt(Transform(ToOpt(in->has_i64_value, in->i64_value)));
    auto [out_has_float_value, out_float_value] =
        FromOpt(Transform(ToOpt(in->has_float_value, in->float_value)));
    auto [out_has_double_value, out_double_value] =
        FromOpt(Transform(ToOpt(in->has_double_value, in->double_value)));
    std::move(callback).Run(mojom::CompatibleStructWithNumerics::New(
        out_has_bool_value, out_bool_value, out_has_u8_value, out_u8_value,
        out_has_u16_value, out_u16_value, out_has_u32_value, out_u32_value,
        out_has_u64_value, out_u64_value, out_has_i8_value, out_i8_value,
        out_has_i16_value, out_i16_value, out_has_i32_value, out_i32_value,
        out_has_i64_value, out_i64_value, out_has_float_value, out_float_value,
        out_has_double_value, out_double_value));
  }

  void MethodWithVersionedArgs(
      MethodWithVersionedArgsCallback callback) override {
    std::move(callback).Run();
  }

  void MethodWithVersionedStruct(
      mojom::VersionedStructV1Ptr in,
      MethodWithVersionedStructCallback callback) override {
    std::move(callback).Run(std::move(in));
  }

  const Receiver<mojom::InterfaceV1> receiver_;
};

enum class CallerVersion {
  kV1,
  kV2,
};

class InterfaceV2Impl : public mojom::InterfaceV2 {
 public:
  explicit InterfaceV2Impl(
      PendingReceiver<mojom::InterfaceV2> receiver,
      absl::optional<CallerVersion> caller_version = absl::nullopt)
      : receiver_(this, std::move(receiver)), caller_version_(caller_version) {}

 private:
  // mojom::InterfaceV2 implementation:
  void MethodWithEnums(absl::optional<mojom::RegularEnum> enum_value,
                       absl::optional<TypemappedEnum> mapped_enum_value,
                       MethodWithEnumsCallback callback) override {
    std::move(callback).Run(Transform(enum_value),
                            Transform(mapped_enum_value));
  }

  void MethodWithStructWithEnums(
      mojom::StructWithEnumsPtr in,
      MethodWithStructWithEnumsCallback callback) override {
    std::move(callback).Run(mojom::StructWithEnums::New(
        Transform(in->enum_value), Transform(in->mapped_enum_value)));
  }

  void MethodWithNumerics(absl::optional<bool> bool_value,
                          absl::optional<uint8_t> u8_value,
                          absl::optional<uint16_t> u16_value,
                          absl::optional<uint32_t> u32_value,
                          absl::optional<uint64_t> u64_value,
                          absl::optional<int8_t> i8_value,
                          absl::optional<int16_t> i16_value,
                          absl::optional<int32_t> i32_value,
                          absl::optional<int64_t> i64_value,
                          absl::optional<float> float_value,
                          absl::optional<double> double_value,
                          MethodWithNumericsCallback callback) override {
    std::move(callback).Run(
        Transform(bool_value), Transform(u8_value), Transform(u16_value),
        Transform(u32_value), Transform(u64_value), Transform(i8_value),
        Transform(i16_value), Transform(i32_value), Transform(i64_value),
        Transform(float_value), Transform(double_value));
  }

  void MethodWithStructWithNumerics(
      mojom::StructWithNumericsPtr in,
      MethodWithStructWithNumericsCallback callback) override {
    std::move(callback).Run(mojom::StructWithNumerics::New(
        Transform(in->bool_value), Transform(in->u8_value),
        Transform(in->u16_value), Transform(in->u32_value),
        Transform(in->u64_value), Transform(in->i8_value),
        Transform(in->i16_value), Transform(in->i32_value),
        Transform(in->i64_value), Transform(in->float_value),
        Transform(in->double_value)));
  }

  void MethodWithVersionedArgs(
      absl::optional<bool> bool_value,
      absl::optional<uint8_t> u8_value,
      absl::optional<uint16_t> u16_value,
      absl::optional<uint32_t> u32_value,
      absl::optional<uint64_t> u64_value,
      absl::optional<int8_t> i8_value,
      absl::optional<int16_t> i16_value,
      absl::optional<int32_t> i32_value,
      absl::optional<int64_t> i64_value,
      absl::optional<float> float_value,
      absl::optional<double> double_value,
      absl::optional<mojom::RegularEnum> enum_value,
      absl::optional<TypemappedEnum> mapped_enum_value,
      MethodWithVersionedArgsCallback callback) override {
    switch (*caller_version_) {
      case CallerVersion::kV1:
        // A caller using the V1 interface will not know about the new
        // arguments, so they should all equal absl::nullopt.
        EXPECT_EQ(absl::nullopt, bool_value);
        EXPECT_EQ(absl::nullopt, u8_value);
        EXPECT_EQ(absl::nullopt, u16_value);
        EXPECT_EQ(absl::nullopt, u32_value);
        EXPECT_EQ(absl::nullopt, u64_value);
        EXPECT_EQ(absl::nullopt, i8_value);
        EXPECT_EQ(absl::nullopt, i16_value);
        EXPECT_EQ(absl::nullopt, i32_value);
        EXPECT_EQ(absl::nullopt, i64_value);
        EXPECT_EQ(absl::nullopt, float_value);
        EXPECT_EQ(absl::nullopt, double_value);
        EXPECT_EQ(absl::nullopt, enum_value);
        EXPECT_EQ(absl::nullopt, mapped_enum_value);
        break;
      case CallerVersion::kV2:
        EXPECT_EQ(true, bool_value);
        EXPECT_EQ(uint8_t{1}, u8_value);
        EXPECT_EQ(uint16_t{2}, u16_value);
        EXPECT_EQ(uint32_t{4}, u32_value);
        EXPECT_EQ(uint64_t{8}, u64_value);
        EXPECT_EQ(int8_t{-16}, i8_value);
        EXPECT_EQ(int16_t{-32}, i16_value);
        EXPECT_EQ(int32_t{-64}, i32_value);
        EXPECT_EQ(int64_t{-128}, i64_value);
        EXPECT_EQ(256.0f, float_value);
        EXPECT_EQ(-512.0, double_value);
        EXPECT_EQ(mojom::RegularEnum::kThisValue, enum_value);
        EXPECT_EQ(TypemappedEnum::kValueTwo, mapped_enum_value);
        break;
    }
    std::move(callback).Run(
        false, uint8_t{128}, uint16_t{64}, uint32_t{32}, uint64_t{16},
        int8_t{-8}, int16_t{-4}, int32_t{-2}, int64_t{-1}, -0.5f, 0.25,
        mojom::RegularEnum::kThatValue, TypemappedEnum::kValueOne);
  }

  void MethodWithVersionedStruct(
      mojom::VersionedStructV2Ptr in,
      MethodWithVersionedStructCallback callback) override {
    switch (*caller_version_) {
      case CallerVersion::kV1:
        // A caller using the V1 interface will not know about the new
        // arguments, so they should all equal absl::nullopt.
        EXPECT_EQ(absl::nullopt, in->bool_value);
        EXPECT_EQ(absl::nullopt, in->u8_value);
        EXPECT_EQ(absl::nullopt, in->u16_value);
        EXPECT_EQ(absl::nullopt, in->u32_value);
        EXPECT_EQ(absl::nullopt, in->u64_value);
        EXPECT_EQ(absl::nullopt, in->i8_value);
        EXPECT_EQ(absl::nullopt, in->i16_value);
        EXPECT_EQ(absl::nullopt, in->i32_value);
        EXPECT_EQ(absl::nullopt, in->i64_value);
        EXPECT_EQ(absl::nullopt, in->float_value);
        EXPECT_EQ(absl::nullopt, in->double_value);
        EXPECT_EQ(absl::nullopt, in->enum_value);
        EXPECT_EQ(absl::nullopt, in->mapped_enum_value);
        break;
      case CallerVersion::kV2:
        EXPECT_EQ(true, in->bool_value);
        EXPECT_EQ(uint8_t{1}, in->u8_value);
        EXPECT_EQ(uint16_t{2}, in->u16_value);
        EXPECT_EQ(uint32_t{4}, in->u32_value);
        EXPECT_EQ(uint64_t{8}, in->u64_value);
        EXPECT_EQ(int8_t{-16}, in->i8_value);
        EXPECT_EQ(int16_t{-32}, in->i16_value);
        EXPECT_EQ(int32_t{-64}, in->i32_value);
        EXPECT_EQ(int64_t{-128}, in->i64_value);
        EXPECT_EQ(256.0f, in->float_value);
        EXPECT_EQ(-512.0, in->double_value);
        EXPECT_EQ(mojom::RegularEnum::kThisValue, in->enum_value);
        EXPECT_EQ(TypemappedEnum::kValueTwo, in->mapped_enum_value);
        break;
    }
    std::move(callback).Run(mojom::VersionedStructV2::New(
        false, uint8_t{128}, uint16_t{64}, uint32_t{32}, uint64_t{16},
        int8_t{-8}, int16_t{-4}, int32_t{-2}, int64_t{-1}, -0.5f, 0.25,
        mojom::RegularEnum::kThatValue, TypemappedEnum::kValueOne));
  }

  const Receiver<mojom::InterfaceV2> receiver_;
  const absl::optional<CallerVersion> caller_version_;
};

class NullableNumericsAndEnums : public ::testing::Test {
  base::test::SingleThreadTaskEnvironment task_environment;
};

TEST_F(NullableNumericsAndEnums, StructWithEnums) {
  {
    auto input = mojom::StructWithEnums::New();
    input->enum_value = absl::nullopt;
    input->mapped_enum_value = absl::nullopt;

    mojom::StructWithEnumsPtr output;
    ASSERT_TRUE(SerializeAndDeserialize<mojom::StructWithEnums>(input, output));

    EXPECT_EQ(absl::nullopt, output->enum_value);
    EXPECT_EQ(absl::nullopt, output->mapped_enum_value);
  }

  {
    auto input = mojom::StructWithEnums::New();
    input->enum_value = mojom::RegularEnum::kThisValue;
    input->mapped_enum_value = absl::nullopt;

    mojom::StructWithEnumsPtr output;
    ASSERT_TRUE(SerializeAndDeserialize<mojom::StructWithEnums>(input, output));

    EXPECT_EQ(mojom::RegularEnum::kThisValue, output->enum_value);
    EXPECT_EQ(absl::nullopt, output->mapped_enum_value);
  }

  {
    auto input = mojom::StructWithEnums::New();
    input->enum_value = absl::nullopt;
    input->mapped_enum_value = TypemappedEnum::kValueOne;

    mojom::StructWithEnumsPtr output;
    ASSERT_TRUE(SerializeAndDeserialize<mojom::StructWithEnums>(input, output));

    EXPECT_EQ(absl::nullopt, output->enum_value);
    EXPECT_EQ(TypemappedEnum::kValueOne, output->mapped_enum_value);
  }

  {
    auto input = mojom::StructWithEnums::New();
    input->enum_value = mojom::RegularEnum::kThatValue;
    input->mapped_enum_value = TypemappedEnum::kValueTwo;

    mojom::StructWithEnumsPtr output;
    ASSERT_TRUE(SerializeAndDeserialize<mojom::StructWithEnums>(input, output));

    EXPECT_EQ(mojom::RegularEnum::kThatValue, output->enum_value);
    EXPECT_EQ(TypemappedEnum::kValueTwo, output->mapped_enum_value);
  }
}

TEST_F(NullableNumericsAndEnums, MethodEnumArgsCompatibility) {
  // Legacy bool+enum calling a receiver using optional<enum>
  {
    mojo::Remote<mojom::InterfaceV1> remote;
    mojo::PendingReceiver<mojom::InterfaceV2> receiver(
        remote.BindNewPipeAndPassReceiver().PassPipe());
    InterfaceV2Impl impl(std::move(receiver));

    {
      base::RunLoop loop;
      remote->MethodWithEnums(
          false, /* ignored */ mojom::RegularEnum::kThisValue, false,
          /* ignored */ TypemappedEnum::kValueOne,
          base::BindLambdaForTesting([&](bool has_enum_value,
                                         mojom::RegularEnum enum_value,
                                         bool has_mapped_enum_value,
                                         TypemappedEnum mapped_enum_value) {
            EXPECT_FALSE(has_enum_value);
            EXPECT_FALSE(has_mapped_enum_value);
            loop.Quit();
          }));
      loop.Run();
    }

    {
      base::RunLoop loop;
      remote->MethodWithEnums(
          true, mojom::RegularEnum::kThisValue, false,
          /* ignored */ TypemappedEnum::kValueOne,
          base::BindLambdaForTesting([&](bool has_enum_value,
                                         mojom::RegularEnum enum_value,
                                         bool has_mapped_enum_value,
                                         TypemappedEnum mapped_enum_value) {
            EXPECT_TRUE(has_enum_value);
            EXPECT_EQ(mojom::RegularEnum::kThatValue, enum_value);
            EXPECT_FALSE(has_mapped_enum_value);
            loop.Quit();
          }));
      loop.Run();
    }

    {
      base::RunLoop loop;
      remote->MethodWithEnums(
          false, /* ignored */ mojom::RegularEnum::kThisValue, true,
          TypemappedEnum::kValueOne,
          base::BindLambdaForTesting([&](bool has_enum_value,
                                         mojom::RegularEnum enum_value,
                                         bool has_mapped_enum_value,
                                         TypemappedEnum mapped_enum_value) {
            EXPECT_FALSE(has_enum_value);
            EXPECT_TRUE(has_mapped_enum_value);
            EXPECT_EQ(TypemappedEnum::kValueTwo, mapped_enum_value);
            loop.Quit();
          }));
      loop.Run();
    }

    {
      base::RunLoop loop;
      remote->MethodWithEnums(
          true, mojom::RegularEnum::kThatValue, true, TypemappedEnum::kValueTwo,
          base::BindLambdaForTesting([&](bool has_enum_value,
                                         mojom::RegularEnum enum_value,
                                         bool has_mapped_enum_value,
                                         TypemappedEnum mapped_enum_value) {
            EXPECT_TRUE(has_enum_value);
            EXPECT_EQ(mojom::RegularEnum::kThisValue, enum_value);
            EXPECT_TRUE(has_mapped_enum_value);
            EXPECT_EQ(TypemappedEnum::kValueOne, mapped_enum_value);
            loop.Quit();
          }));
      loop.Run();
    }
  }

  // optional<enum> calling a receiver using legacy bool+enum.
  {
    mojo::Remote<mojom::InterfaceV2> remote;
    mojo::PendingReceiver<mojom::InterfaceV1> receiver(
        remote.BindNewPipeAndPassReceiver().PassPipe());
    InterfaceV1Impl impl(std::move(receiver));

    {
      base::RunLoop loop;
      remote->MethodWithEnums(
          absl::nullopt, absl::nullopt,
          base::BindLambdaForTesting(
              [&](absl::optional<mojom::RegularEnum> enum_value,
                  absl::optional<TypemappedEnum> mapped_enum_value) {
                EXPECT_EQ(absl::nullopt, enum_value);
                EXPECT_EQ(absl::nullopt, mapped_enum_value);
                loop.Quit();
              }));
      loop.Run();
    }

    {
      base::RunLoop loop;
      remote->MethodWithEnums(
          mojom::RegularEnum::kThisValue, absl::nullopt,
          base::BindLambdaForTesting(
              [&](absl::optional<mojom::RegularEnum> enum_value,
                  absl::optional<TypemappedEnum> mapped_enum_value) {
                EXPECT_EQ(mojom::RegularEnum::kThatValue, enum_value);
                EXPECT_EQ(absl::nullopt, mapped_enum_value);
                loop.Quit();
              }));
      loop.Run();
    }

    {
      base::RunLoop loop;
      remote->MethodWithEnums(
          absl::nullopt, TypemappedEnum::kValueOne,
          base::BindLambdaForTesting(
              [&](absl::optional<mojom::RegularEnum> enum_value,
                  absl::optional<TypemappedEnum> mapped_enum_value) {
                EXPECT_EQ(absl::nullopt, enum_value);
                EXPECT_EQ(TypemappedEnum::kValueTwo, mapped_enum_value);
                loop.Quit();
              }));
      loop.Run();
    }

    {
      base::RunLoop loop;
      remote->MethodWithEnums(
          mojom::RegularEnum::kThatValue, TypemappedEnum::kValueTwo,
          base::BindLambdaForTesting(
              [&](absl::optional<mojom::RegularEnum> enum_value,
                  absl::optional<TypemappedEnum> mapped_enum_value) {
                EXPECT_EQ(mojom::RegularEnum::kThisValue, enum_value);
                EXPECT_EQ(TypemappedEnum::kValueOne, mapped_enum_value);
                loop.Quit();
              }));
      loop.Run();
    }
  }
}

TEST_F(NullableNumericsAndEnums, MethodStructWithEnumsCompatibility) {
  // Legacy bool+enum calling a receiver using optional<enum>
  {
    mojo::Remote<mojom::InterfaceV1> remote;
    mojo::PendingReceiver<mojom::InterfaceV2> receiver(
        remote.BindNewPipeAndPassReceiver().PassPipe());
    InterfaceV2Impl impl(std::move(receiver));

    {
      base::RunLoop loop;
      remote->MethodWithStructWithEnums(
          mojom::CompatibleStructWithEnums::New(
              false, /* ignored */ mojom::RegularEnum::kThisValue, false,
              /* ignored */ TypemappedEnum::kValueOne),
          base::BindLambdaForTesting(
              [&](mojom::CompatibleStructWithEnumsPtr out) {
                EXPECT_FALSE(out->has_enum_value);
                EXPECT_FALSE(out->has_mapped_enum_value);
                loop.Quit();
              }));
      loop.Run();
    }

    {
      base::RunLoop loop;
      remote->MethodWithStructWithEnums(
          mojom::CompatibleStructWithEnums::New(
              true, mojom::RegularEnum::kThisValue, false,
              /* ignored */ TypemappedEnum::kValueOne),
          base::BindLambdaForTesting(
              [&](mojom::CompatibleStructWithEnumsPtr out) {
                EXPECT_TRUE(out->has_enum_value);
                EXPECT_EQ(mojom::RegularEnum::kThatValue, out->enum_value);
                EXPECT_FALSE(out->has_mapped_enum_value);
                loop.Quit();
              }));
      loop.Run();
    }

    {
      base::RunLoop loop;
      remote->MethodWithStructWithEnums(
          mojom::CompatibleStructWithEnums::New(
              false, /* ignored */ mojom::RegularEnum::kThisValue, true,
              TypemappedEnum::kValueOne),
          base::BindLambdaForTesting(
              [&](mojom::CompatibleStructWithEnumsPtr out) {
                EXPECT_FALSE(out->has_enum_value);
                EXPECT_TRUE(out->has_mapped_enum_value);
                EXPECT_EQ(TypemappedEnum::kValueTwo, out->mapped_enum_value);
                loop.Quit();
              }));
      loop.Run();
    }

    {
      base::RunLoop loop;
      remote->MethodWithStructWithEnums(
          mojom::CompatibleStructWithEnums::New(
              true, mojom::RegularEnum::kThatValue, true,
              TypemappedEnum::kValueTwo),
          base::BindLambdaForTesting(
              [&](mojom::CompatibleStructWithEnumsPtr out) {
                EXPECT_TRUE(out->has_enum_value);
                EXPECT_EQ(mojom::RegularEnum::kThisValue, out->enum_value);
                EXPECT_TRUE(out->has_mapped_enum_value);
                EXPECT_EQ(TypemappedEnum::kValueOne, out->mapped_enum_value);
                loop.Quit();
              }));
      loop.Run();
    }
  }

  // optional<enum> calling a receiver using legacy bool+enum.
  {
    mojo::Remote<mojom::InterfaceV2> remote;
    mojo::PendingReceiver<mojom::InterfaceV1> receiver(
        remote.BindNewPipeAndPassReceiver().PassPipe());
    InterfaceV1Impl impl(std::move(receiver));

    {
      base::RunLoop loop;
      remote->MethodWithStructWithEnums(
          mojom::StructWithEnums::New(absl::nullopt, absl::nullopt),
          base::BindLambdaForTesting([&](mojom::StructWithEnumsPtr out) {
            EXPECT_EQ(absl::nullopt, out->enum_value);
            EXPECT_EQ(absl::nullopt, out->mapped_enum_value);
            loop.Quit();
          }));
      loop.Run();
    }

    {
      base::RunLoop loop;
      remote->MethodWithStructWithEnums(
          mojom::StructWithEnums::New(mojom::RegularEnum::kThisValue,
                                      absl::nullopt),
          base::BindLambdaForTesting([&](mojom::StructWithEnumsPtr out) {
            EXPECT_EQ(mojom::RegularEnum::kThatValue, out->enum_value);
            EXPECT_EQ(absl::nullopt, out->mapped_enum_value);
            loop.Quit();
          }));
      loop.Run();
    }

    {
      base::RunLoop loop;
      remote->MethodWithStructWithEnums(
          mojom::StructWithEnums::New(absl::nullopt, TypemappedEnum::kValueOne),
          base::BindLambdaForTesting([&](mojom::StructWithEnumsPtr out) {
            EXPECT_EQ(absl::nullopt, out->enum_value);
            EXPECT_EQ(TypemappedEnum::kValueTwo, out->mapped_enum_value);
            loop.Quit();
          }));
      loop.Run();
    }

    {
      base::RunLoop loop;
      remote->MethodWithStructWithEnums(
          mojom::StructWithEnums::New(mojom::RegularEnum::kThatValue,
                                      TypemappedEnum::kValueTwo),
          base::BindLambdaForTesting([&](mojom::StructWithEnumsPtr out) {
            EXPECT_EQ(mojom::RegularEnum::kThisValue, out->enum_value);
            EXPECT_EQ(TypemappedEnum::kValueOne, out->mapped_enum_value);
            loop.Quit();
          }));
      loop.Run();
    }
  }
}

TEST_F(NullableNumericsAndEnums, StructWithNumerics) {
  {
    auto input = mojom::StructWithNumerics::New();
    input->bool_value = true;
    input->u8_value = absl::nullopt;
    input->u16_value = 16;
    input->u32_value = absl::nullopt;
    input->u64_value = 64;
    input->i8_value = -8;
    input->i16_value = absl::nullopt;
    input->i32_value = -32;
    input->i64_value = absl::nullopt;
    input->float_value = absl::nullopt;
    input->double_value = -64.0;

    mojom::StructWithNumericsPtr output;
    ASSERT_TRUE(
        SerializeAndDeserialize<mojom::StructWithNumerics>(input, output));

    EXPECT_EQ(true, output->bool_value);
    EXPECT_EQ(absl::nullopt, output->u8_value);
    EXPECT_EQ(16u, output->u16_value);
    EXPECT_EQ(absl::nullopt, output->u32_value);
    EXPECT_EQ(64u, output->u64_value);
    EXPECT_EQ(-8, output->i8_value);
    EXPECT_EQ(absl::nullopt, output->i16_value);
    EXPECT_EQ(-32, output->i32_value);
    EXPECT_EQ(absl::nullopt, output->i64_value);
    EXPECT_EQ(absl::nullopt, output->float_value);
    EXPECT_EQ(-64.0, output->double_value);
  }

  {
    auto input = mojom::StructWithNumerics::New();
    input->bool_value = absl::nullopt;
    input->u8_value = 8;
    input->u16_value = absl::nullopt;
    input->u32_value = 32;
    input->u64_value = absl::nullopt;
    input->i8_value = absl::nullopt;
    input->i16_value = -16;
    input->i32_value = absl::nullopt;
    input->i64_value = -64;
    input->float_value = -32.0f;
    input->double_value = absl::nullopt;

    mojom::StructWithNumericsPtr output;
    ASSERT_TRUE(
        SerializeAndDeserialize<mojom::StructWithNumerics>(input, output));

    EXPECT_EQ(absl::nullopt, output->bool_value);
    EXPECT_EQ(8u, output->u8_value);
    EXPECT_EQ(absl::nullopt, output->u16_value);
    EXPECT_EQ(32u, output->u32_value);
    EXPECT_EQ(absl::nullopt, output->u64_value);
    EXPECT_EQ(absl::nullopt, output->i8_value);
    EXPECT_EQ(-16, output->i16_value);
    EXPECT_EQ(absl::nullopt, output->i32_value);
    EXPECT_EQ(-64, output->i64_value);
    EXPECT_EQ(-32.0f, output->float_value);
    EXPECT_EQ(absl::nullopt, output->double_value);
  }
}

TEST_F(NullableNumericsAndEnums, MethodNumericArgsCompatibility) {
  // Legacy bool+enum calling a receiver using optional<enum>
  {
    mojo::Remote<mojom::InterfaceV1> remote;
    mojo::PendingReceiver<mojom::InterfaceV2> receiver(
        remote.BindNewPipeAndPassReceiver().PassPipe());
    InterfaceV2Impl impl(std::move(receiver));

    {
      base::RunLoop loop;
      remote->MethodWithNumerics(
          true, true, false, /* ignored */ uint8_t{0}, true, uint16_t{16},
          false,
          /* ignored */ uint32_t{0}, true, uint64_t{64}, true, int8_t{-8},
          false, /* ignored */ int16_t{0}, true, int32_t{-32}, false,
          int64_t{0}, false, /* ignored */ 0.0f, true, -64.0,
          base::BindLambdaForTesting(
              [&](bool has_bool_value, bool bool_value, bool has_u8_value,
                  uint8_t u8_value, bool has_u16_value, uint16_t u16_value,
                  bool has_u32_value, uint32_t u32_value, bool has_u64_value,
                  uint64_t u64_value, bool has_i8_value, int8_t i8_value,
                  bool has_i16_value, int16_t i16_value, bool has_i32_value,
                  int32_t i32_value, bool has_i64_value, int64_t i64_value,
                  bool has_float_value, float float_value,
                  bool has_double_value, double double_value) {
                EXPECT_TRUE(has_bool_value);
                EXPECT_EQ(false, bool_value);
                EXPECT_FALSE(has_u8_value);
                EXPECT_TRUE(has_u16_value);
                // Note: the seemingly more obvious ~uint16_t{16} is not used
                // here because using ~ when sizeof(integer) < sizeof(int)
                // automatically promotes to an int. 🙃
                EXPECT_EQ(uint16_t{0xffef}, u16_value);
                EXPECT_FALSE(has_u32_value);
                EXPECT_TRUE(has_u64_value);
                EXPECT_EQ(~uint64_t{64}, u64_value);
                EXPECT_TRUE(has_i8_value);
                EXPECT_EQ(8, i8_value);
                EXPECT_FALSE(has_i16_value);
                EXPECT_TRUE(has_i32_value);
                EXPECT_EQ(32, i32_value);
                EXPECT_FALSE(has_i64_value);
                EXPECT_FALSE(has_float_value);
                EXPECT_TRUE(has_double_value);
                EXPECT_EQ(128.0, double_value);
                loop.Quit();
              }));
      loop.Run();
    }

    {
      base::RunLoop loop;
      remote->MethodWithNumerics(
          false, /* ignored */ false, true, uint8_t{8}, false,
          /* ignored */ uint16_t{0}, true, uint32_t{32}, false,
          /* ignored */ uint64_t{0}, false,
          /* ignored */ int8_t{0}, true, int16_t{-16}, false,
          /* ignored */ int32_t{0}, true, int64_t{-64}, true, -32.0f, false,
          /* ignored */ -0.0,
          base::BindLambdaForTesting(
              [&](bool has_bool_value, bool bool_value, bool has_u8_value,
                  uint8_t u8_value, bool has_u16_value, uint16_t u16_value,
                  bool has_u32_value, uint32_t u32_value, bool has_u64_value,
                  uint64_t u64_value, bool has_i8_value, int8_t i8_value,
                  bool has_i16_value, int16_t i16_value, bool has_i32_value,
                  int32_t i32_value, bool has_i64_value, int64_t i64_value,
                  bool has_float_value, float float_value,
                  bool has_double_value, double double_value) {
                EXPECT_FALSE(has_bool_value);
                EXPECT_TRUE(has_u8_value);
                // Note: the seemingly more obvious ~uint8_t{8} is not used
                // here because using ~ when sizeof(integer) < sizeof(int)
                // automatically promotes to an int. 🙃
                EXPECT_EQ(uint8_t{0xf7}, u8_value);
                EXPECT_FALSE(has_u16_value);
                EXPECT_TRUE(has_u32_value);
                EXPECT_EQ(~uint32_t{32}, u32_value);
                EXPECT_FALSE(has_u64_value);
                EXPECT_FALSE(has_i8_value);
                EXPECT_TRUE(has_i16_value);
                EXPECT_EQ(16, i16_value);
                EXPECT_FALSE(has_i32_value);
                EXPECT_TRUE(has_i64_value);
                EXPECT_EQ(64, i64_value);
                EXPECT_TRUE(has_float_value);
                EXPECT_EQ(64.0, float_value);
                EXPECT_FALSE(has_double_value);
                loop.Quit();
              }));
      loop.Run();
    }
  }

  // optional<enum> calling a receiver using legacy bool+enum.
  {
    mojo::Remote<mojom::InterfaceV2> remote;
    mojo::PendingReceiver<mojom::InterfaceV1> receiver(
        remote.BindNewPipeAndPassReceiver().PassPipe());
    InterfaceV1Impl impl(std::move(receiver));

    {
      base::RunLoop loop;
      remote->MethodWithNumerics(
          true, absl::nullopt, uint16_t{16}, absl::nullopt, uint64_t{64},
          int8_t{-8}, absl::nullopt, int32_t{-32}, absl::nullopt, absl::nullopt,
          -64.0,
          base::BindLambdaForTesting([&](absl::optional<bool> bool_value,
                                         absl::optional<uint8_t> u8_value,
                                         absl::optional<uint16_t> u16_value,
                                         absl::optional<uint32_t> u32_value,
                                         absl::optional<uint64_t> u64_value,
                                         absl::optional<int8_t> i8_value,
                                         absl::optional<int16_t> i16_value,
                                         absl::optional<int32_t> i32_value,
                                         absl::optional<int64_t> i64_value,
                                         absl::optional<float> float_value,
                                         absl::optional<double> double_value) {
            EXPECT_EQ(false, bool_value);
            EXPECT_EQ(absl::nullopt, u8_value);
            // Note: the seemingly more obvious ~uint16_t{16} is not used
            // here because using ~ when sizeof(integer) < sizeof(int)
            // automatically promotes to an int. 🙃
            EXPECT_EQ(uint16_t{0xffef}, u16_value);
            EXPECT_EQ(absl::nullopt, u32_value);
            EXPECT_EQ(~uint64_t{64}, u64_value);
            EXPECT_EQ(8, i8_value);
            EXPECT_EQ(absl::nullopt, i16_value);
            EXPECT_EQ(32, i32_value);
            EXPECT_EQ(absl::nullopt, i64_value);
            EXPECT_EQ(absl::nullopt, float_value);
            EXPECT_EQ(128.0, double_value);
            loop.Quit();
          }));
      loop.Run();
    }

    {
      base::RunLoop loop;
      remote->MethodWithNumerics(
          absl::nullopt, uint8_t{8}, absl::nullopt, uint32_t{32}, absl::nullopt,
          absl::nullopt, int16_t{-16}, absl::nullopt, int64_t{-64}, -32.0f,
          absl::nullopt,
          base::BindLambdaForTesting([&](absl::optional<bool> bool_value,
                                         absl::optional<uint8_t> u8_value,
                                         absl::optional<uint16_t> u16_value,
                                         absl::optional<uint32_t> u32_value,
                                         absl::optional<uint64_t> u64_value,
                                         absl::optional<int8_t> i8_value,
                                         absl::optional<int16_t> i16_value,
                                         absl::optional<int32_t> i32_value,
                                         absl::optional<int64_t> i64_value,
                                         absl::optional<float> float_value,
                                         absl::optional<double> double_value) {
            EXPECT_EQ(absl::nullopt, bool_value);
            // Note: the seemingly more obvious ~uint8_t{8} is not used
            // here because using ~ when sizeof(integer) < sizeof(int)
            // automatically promotes to an int. 🙃
            EXPECT_EQ(uint8_t{0xf7}, u8_value);
            EXPECT_EQ(absl::nullopt, u16_value);
            EXPECT_EQ(~uint32_t{32}, u32_value);
            EXPECT_EQ(absl::nullopt, u64_value);
            EXPECT_EQ(absl::nullopt, i8_value);
            EXPECT_EQ(16, i16_value);
            EXPECT_EQ(absl::nullopt, i32_value);
            EXPECT_EQ(64, i64_value);
            EXPECT_EQ(64.0, float_value);
            EXPECT_EQ(absl::nullopt, double_value);
            loop.Quit();
          }));
      loop.Run();
    }
  }
}

TEST_F(NullableNumericsAndEnums, MethodStructWithNumericsCompatibility) {
  // Legacy bool+enum calling a receiver using optional<enum>
  {
    mojo::Remote<mojom::InterfaceV1> remote;
    mojo::PendingReceiver<mojom::InterfaceV2> receiver(
        remote.BindNewPipeAndPassReceiver().PassPipe());
    InterfaceV2Impl impl(std::move(receiver));

    {
      base::RunLoop loop;
      remote->MethodWithStructWithNumerics(
          mojom::CompatibleStructWithNumerics::New(
              true, true, false, /* ignored */ uint8_t{0}, true, uint16_t{16},
              false,
              /* ignored */ uint32_t{0}, true, uint64_t{64}, true, int8_t{-8},
              false, /* ignored */ int16_t{0}, true, int32_t{-32}, false,
              int64_t{0}, false, /* ignored */ 0.0f, true, -64.0),
          base::BindLambdaForTesting(
              [&](mojom::CompatibleStructWithNumericsPtr out) {
                EXPECT_TRUE(out->has_bool_value);
                EXPECT_EQ(false, out->bool_value);
                EXPECT_FALSE(out->has_u8_value);
                EXPECT_TRUE(out->has_u16_value);
                // Note: the seemingly more obvious ~uint16_t{16} is not used
                // here because using ~ when sizeof(integer) < sizeof(int)
                // automatically promotes to an int. 🙃
                EXPECT_EQ(uint16_t{0xffef}, out->u16_value);
                EXPECT_FALSE(out->has_u32_value);
                EXPECT_TRUE(out->has_u64_value);
                EXPECT_EQ(~uint64_t{64}, out->u64_value);
                EXPECT_TRUE(out->has_i8_value);
                EXPECT_EQ(8, out->i8_value);
                EXPECT_FALSE(out->has_i16_value);
                EXPECT_TRUE(out->has_i32_value);
                EXPECT_EQ(32, out->i32_value);
                EXPECT_FALSE(out->has_i64_value);
                EXPECT_FALSE(out->has_float_value);
                EXPECT_TRUE(out->has_double_value);
                EXPECT_EQ(128.0, out->double_value);
                loop.Quit();
              }));
      loop.Run();
    }

    {
      base::RunLoop loop;
      remote->MethodWithStructWithNumerics(
          mojom::CompatibleStructWithNumerics::New(
              false, /* ignored */ false, true, uint8_t{8}, false,
              /* ignored */ uint16_t{0}, true, uint32_t{32}, false,
              /* ignored */ uint64_t{0}, false,
              /* ignored */ int8_t{0}, true, int16_t{-16}, false,
              /* ignored */ int32_t{0}, true, int64_t{-64}, true, -32.0f, false,
              /* ignored */ -0.0),
          base::BindLambdaForTesting(
              [&](mojom::CompatibleStructWithNumericsPtr out) {
                EXPECT_FALSE(out->has_bool_value);
                EXPECT_TRUE(out->has_u8_value);
                // Note: the seemingly more obvious ~uint8_t{8} is not used
                // here because using ~ when sizeof(integer) < sizeof(int)
                // automatically promotes to an int. 🙃
                EXPECT_EQ(uint8_t{0xf7}, out->u8_value);
                EXPECT_FALSE(out->has_u16_value);
                EXPECT_TRUE(out->has_u32_value);
                EXPECT_EQ(~uint32_t{32}, out->u32_value);
                EXPECT_FALSE(out->has_u64_value);
                EXPECT_FALSE(out->has_i8_value);
                EXPECT_TRUE(out->has_i16_value);
                EXPECT_EQ(16, out->i16_value);
                EXPECT_FALSE(out->has_i32_value);
                EXPECT_TRUE(out->has_i64_value);
                EXPECT_EQ(64, out->i64_value);
                EXPECT_TRUE(out->has_float_value);
                EXPECT_EQ(64.0, out->float_value);
                EXPECT_FALSE(out->has_double_value);
                loop.Quit();
              }));
      loop.Run();
    }
  }

  // optional<enum> calling a receiver using legacy bool+enum.
  {
    mojo::Remote<mojom::InterfaceV2> remote;
    mojo::PendingReceiver<mojom::InterfaceV1> receiver(
        remote.BindNewPipeAndPassReceiver().PassPipe());
    InterfaceV1Impl impl(std::move(receiver));

    {
      base::RunLoop loop;
      remote->MethodWithStructWithNumerics(
          mojom::StructWithNumerics::New(
              true, absl::nullopt, uint16_t{16}, absl::nullopt, uint64_t{64},
              int8_t{-8}, absl::nullopt, int32_t{-32}, absl::nullopt,
              absl::nullopt, -64.0),
          base::BindLambdaForTesting([&](mojom::StructWithNumericsPtr out) {
            EXPECT_EQ(false, out->bool_value);
            EXPECT_EQ(absl::nullopt, out->u8_value);
            // Note: the seemingly more obvious ~uint16_t{16} is not used
            // here because using ~ when sizeof(integer) < sizeof(int)
            // automatically promotes to an int. 🙃
            EXPECT_EQ(uint16_t{0xffef}, out->u16_value);
            EXPECT_EQ(absl::nullopt, out->u32_value);
            EXPECT_EQ(~uint64_t{64}, out->u64_value);
            EXPECT_EQ(8, out->i8_value);
            EXPECT_EQ(absl::nullopt, out->i16_value);
            EXPECT_EQ(32, out->i32_value);
            EXPECT_EQ(absl::nullopt, out->i64_value);
            EXPECT_EQ(absl::nullopt, out->float_value);
            EXPECT_EQ(128.0, out->double_value);
            loop.Quit();
          }));
      loop.Run();
    }

    {
      base::RunLoop loop;
      remote->MethodWithStructWithNumerics(
          mojom::StructWithNumerics::New(
              absl::nullopt, uint8_t{8}, absl::nullopt, uint32_t{32},
              absl::nullopt, absl::nullopt, int16_t{-16}, absl::nullopt,
              int64_t{-64}, -32.0f, absl::nullopt),
          base::BindLambdaForTesting([&](mojom::StructWithNumericsPtr out) {
            EXPECT_EQ(absl::nullopt, out->bool_value);
            // Note: the seemingly more obvious ~uint8_t{8} is not used
            // here because using ~ when sizeof(integer) < sizeof(int)
            // automatically promotes to an int. 🙃
            EXPECT_EQ(uint8_t{0xf7}, out->u8_value);
            EXPECT_EQ(absl::nullopt, out->u16_value);
            EXPECT_EQ(~uint32_t{32}, out->u32_value);
            EXPECT_EQ(absl::nullopt, out->u64_value);
            EXPECT_EQ(absl::nullopt, out->i8_value);
            EXPECT_EQ(16, out->i16_value);
            EXPECT_EQ(absl::nullopt, out->i32_value);
            EXPECT_EQ(64, out->i64_value);
            EXPECT_EQ(64.0, out->float_value);
            EXPECT_EQ(absl::nullopt, out->double_value);
            loop.Quit();
          }));
      loop.Run();
    }
  }
}

TEST_F(NullableNumericsAndEnums, Versioning) {
  // Baseline: V1 to V1.
  {
    mojo::Remote<mojom::InterfaceV1> remote;
    mojo::PendingReceiver<mojom::InterfaceV1> receiver(
        remote.BindNewPipeAndPassReceiver().PassPipe());
    InterfaceV1Impl impl(std::move(receiver));

    {
      base::RunLoop loop;
      remote->MethodWithVersionedArgs(
          base::BindLambdaForTesting([&]() { loop.Quit(); }));
      loop.Run();
    }

    {
      auto expected = mojom::VersionedStructV1::New();
      base::RunLoop loop;
      remote->MethodWithVersionedStruct(
          expected.Clone(),
          base::BindLambdaForTesting([&](mojom::VersionedStructV1Ptr out) {
            EXPECT_EQ(expected, out);
            loop.Quit();
          }));
      loop.Run();
    }
  }

  // V1 to V2.
  {
    mojo::Remote<mojom::InterfaceV1> remote;
    mojo::PendingReceiver<mojom::InterfaceV2> receiver(
        remote.BindNewPipeAndPassReceiver().PassPipe());
    InterfaceV2Impl impl(std::move(receiver), CallerVersion::kV1);

    {
      base::RunLoop loop;
      remote->MethodWithVersionedArgs(
          base::BindLambdaForTesting([&]() { loop.Quit(); }));
      loop.Run();
    }

    {
      auto expected = mojom::VersionedStructV1::New();
      base::RunLoop loop;
      remote->MethodWithVersionedStruct(
          expected.Clone(),
          base::BindLambdaForTesting([&](mojom::VersionedStructV1Ptr out) {
            EXPECT_EQ(expected, out);
            loop.Quit();
          }));
      loop.Run();
    }
  }

  // V2 to V1.
  {
    mojo::Remote<mojom::InterfaceV2> remote;
    mojo::PendingReceiver<mojom::InterfaceV1> receiver(
        remote.BindNewPipeAndPassReceiver().PassPipe());
    InterfaceV1Impl impl(std::move(receiver));

    {
      base::RunLoop loop;
      remote->MethodWithVersionedArgs(
          true, uint8_t{1}, uint16_t{2}, uint32_t{4}, uint64_t{8}, int8_t{-16},
          int16_t{-32}, int32_t{-64}, int64_t{-128}, 256.0f, -512.0,
          mojom::RegularEnum::kThisValue, TypemappedEnum::kValueTwo,
          base::BindLambdaForTesting(
              [&](absl::optional<bool> out_bool_value,
                  absl::optional<uint8_t> out_u8_value,
                  absl::optional<uint16_t> out_u16_value,
                  absl::optional<uint32_t> out_u32_value,
                  absl::optional<uint64_t> out_u64_value,
                  absl::optional<int8_t> out_i8_value,
                  absl::optional<int16_t> out_i16_value,
                  absl::optional<int32_t> out_i32_value,
                  absl::optional<int64_t> out_i64_value,
                  absl::optional<float> out_float_value,
                  absl::optional<double> out_double_value,
                  absl::optional<mojom::RegularEnum> out_enum_value,
                  absl::optional<TypemappedEnum> out_mapped_enum_value) {
                // An implementation based on the V1 interface will not know
                // about the new arguments, so they should all equal
                // absl::nullopt.
                EXPECT_EQ(absl::nullopt, out_bool_value);
                EXPECT_EQ(absl::nullopt, out_u8_value);
                EXPECT_EQ(absl::nullopt, out_u16_value);
                EXPECT_EQ(absl::nullopt, out_u32_value);
                EXPECT_EQ(absl::nullopt, out_u64_value);
                EXPECT_EQ(absl::nullopt, out_i8_value);
                EXPECT_EQ(absl::nullopt, out_i16_value);
                EXPECT_EQ(absl::nullopt, out_i32_value);
                EXPECT_EQ(absl::nullopt, out_i64_value);
                EXPECT_EQ(absl::nullopt, out_float_value);
                EXPECT_EQ(absl::nullopt, out_double_value);
                EXPECT_EQ(absl::nullopt, out_enum_value);
                EXPECT_EQ(absl::nullopt, out_mapped_enum_value);
                loop.Quit();
              }));
      loop.Run();
    }

    {
      base::RunLoop loop;
      remote->MethodWithVersionedStruct(
          mojom::VersionedStructV2::New(
              true, uint8_t{1}, uint16_t{2}, uint32_t{4}, uint64_t{8},
              int8_t{-16}, int16_t{-32}, int32_t{-64}, int64_t{-128}, 256.0f,
              -512.0, mojom::RegularEnum::kThisValue,
              TypemappedEnum::kValueTwo),
          base::BindLambdaForTesting([&](mojom::VersionedStructV2Ptr out) {
            // An implementation based on the V1 interface will not know
            // about the new arguments, so they should all equal
            // absl::nullopt.
            EXPECT_EQ(absl::nullopt, out->bool_value);
            EXPECT_EQ(absl::nullopt, out->u8_value);
            EXPECT_EQ(absl::nullopt, out->u16_value);
            EXPECT_EQ(absl::nullopt, out->u32_value);
            EXPECT_EQ(absl::nullopt, out->u64_value);
            EXPECT_EQ(absl::nullopt, out->i8_value);
            EXPECT_EQ(absl::nullopt, out->i16_value);
            EXPECT_EQ(absl::nullopt, out->i32_value);
            EXPECT_EQ(absl::nullopt, out->i64_value);
            EXPECT_EQ(absl::nullopt, out->float_value);
            EXPECT_EQ(absl::nullopt, out->double_value);
            EXPECT_EQ(absl::nullopt, out->enum_value);
            EXPECT_EQ(absl::nullopt, out->mapped_enum_value);
            loop.Quit();
          }));
      loop.Run();
    }
  }

  // V2 to V2.
  {
    mojo::Remote<mojom::InterfaceV2> remote;
    mojo::PendingReceiver<mojom::InterfaceV2> receiver(
        remote.BindNewPipeAndPassReceiver().PassPipe());
    InterfaceV2Impl impl(std::move(receiver), CallerVersion::kV2);

    {
      base::RunLoop loop;
      remote->MethodWithVersionedArgs(
          true, uint8_t{1}, uint16_t{2}, uint32_t{4}, uint64_t{8}, int8_t{-16},
          int16_t{-32}, int32_t{-64}, int64_t{-128}, 256.0f, -512.0,
          mojom::RegularEnum::kThisValue, TypemappedEnum::kValueTwo,
          base::BindLambdaForTesting(
              [&](absl::optional<bool> out_bool_value,
                  absl::optional<uint8_t> out_u8_value,
                  absl::optional<uint16_t> out_u16_value,
                  absl::optional<uint32_t> out_u32_value,
                  absl::optional<uint64_t> out_u64_value,
                  absl::optional<int8_t> out_i8_value,
                  absl::optional<int16_t> out_i16_value,
                  absl::optional<int32_t> out_i32_value,
                  absl::optional<int64_t> out_i64_value,
                  absl::optional<float> out_float_value,
                  absl::optional<double> out_double_value,
                  absl::optional<mojom::RegularEnum> out_enum_value,
                  absl::optional<TypemappedEnum> out_mapped_enum_value) {
                EXPECT_EQ(false, out_bool_value);
                EXPECT_EQ(uint8_t{128}, out_u8_value);
                EXPECT_EQ(uint16_t{64}, out_u16_value);
                EXPECT_EQ(uint32_t{32}, out_u32_value);
                EXPECT_EQ(uint64_t{16}, out_u64_value);
                EXPECT_EQ(int8_t{-8}, out_i8_value);
                EXPECT_EQ(int16_t{-4}, out_i16_value);
                EXPECT_EQ(int32_t{-2}, out_i32_value);
                EXPECT_EQ(int32_t{-1}, out_i64_value);
                EXPECT_EQ(-0.5f, out_float_value);
                EXPECT_EQ(0.25, out_double_value);
                EXPECT_EQ(mojom::RegularEnum::kThatValue, out_enum_value);
                EXPECT_EQ(TypemappedEnum::kValueOne, out_mapped_enum_value);
                loop.Quit();
              }));
      loop.Run();
    }

    {
      base::RunLoop loop;
      remote->MethodWithVersionedStruct(
          mojom::VersionedStructV2::New(
              true, uint8_t{1}, uint16_t{2}, uint32_t{4}, uint64_t{8},
              int8_t{-16}, int16_t{-32}, int32_t{-64}, int64_t{-128}, 256.0f,
              -512.0, mojom::RegularEnum::kThisValue,
              TypemappedEnum::kValueTwo),
          base::BindLambdaForTesting([&](mojom::VersionedStructV2Ptr out) {
            EXPECT_EQ(false, out->bool_value);
            EXPECT_EQ(uint8_t{128}, out->u8_value);
            EXPECT_EQ(uint16_t{64}, out->u16_value);
            EXPECT_EQ(uint32_t{32}, out->u32_value);
            EXPECT_EQ(uint64_t{16}, out->u64_value);
            EXPECT_EQ(int8_t{-8}, out->i8_value);
            EXPECT_EQ(int16_t{-4}, out->i16_value);
            EXPECT_EQ(int32_t{-2}, out->i32_value);
            EXPECT_EQ(int32_t{-1}, out->i64_value);
            EXPECT_EQ(-0.5f, out->float_value);
            EXPECT_EQ(0.25, out->double_value);
            EXPECT_EQ(mojom::RegularEnum::kThatValue, out->enum_value);
            EXPECT_EQ(TypemappedEnum::kValueOne, out->mapped_enum_value);
            loop.Quit();
          }));
      loop.Run();
    }
  }
}

}  // namespace
}  // namespace mojo::test::nullable_numerics_and_enums_unittest
