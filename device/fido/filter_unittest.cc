// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/filter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace fido_filter {
namespace {

TEST(FidoFilter, Basic) {
  static const uint8_t kCredIDBytes[] = {1, 2, 3, 4};
  const auto cred_id = std::pair<IDType, base::span<const uint8_t>>(
      IDType::CREDENTIAL_ID, kCredIDBytes);
  const auto empty_cred_id = std::pair<IDType, base::span<const uint8_t>>(
      IDType::CREDENTIAL_ID, base::span<const uint8_t>());

  static const struct {
    const char* filter;
    Operation op;
    base::StringPiece rp_id;
    absl::optional<base::StringPiece> device;
    absl::optional<std::pair<IDType, base::span<const uint8_t>>> id;
    Action expected;
  } kTests[] = {
      {
          "",
          Operation::MAKE_CREDENTIAL,
          "example.com",
          absl::nullopt,
          absl::nullopt,
          Action::ALLOW,
      },
      {
          R"({"filters": []})",
          Operation::MAKE_CREDENTIAL,
          "example.com",
          absl::nullopt,
          absl::nullopt,
          Action::ALLOW,
      },
      {
          R"({"filters": [{
            "operation": "mc",
            "rp_id": "*",
            "action": "block",
          }]})",
          Operation::MAKE_CREDENTIAL,
          "example.com",
          absl::nullopt,
          absl::nullopt,
          Action::BLOCK,
      },
      {
          R"({"filters": [{
            "operation": "m*",
            "rp_id": "*",
            "action": "block",
          }]})",
          Operation::MAKE_CREDENTIAL,
          "example.com",
          absl::nullopt,
          absl::nullopt,
          Action::BLOCK,
      },
      {
          R"({"filters": [{
            "operation": "*",
            "rp_id": "*",
            "action": "block",
          }]})",
          Operation::MAKE_CREDENTIAL,
          "example.com",
          absl::nullopt,
          absl::nullopt,
          Action::BLOCK,
      },
      {
          R"({"filters": [{
            "operation": "mc",
            "rp_id": "*",
            "action": "no-attestation",
          }]})",
          Operation::MAKE_CREDENTIAL,
          "example.com",
          absl::nullopt,
          absl::nullopt,
          Action::NO_ATTESTATION,
      },
      {
          R"({"filters": [{
            "rp_id": "example.com",
            "action": "block",
          }]})",
          Operation::MAKE_CREDENTIAL,
          "example.com",
          absl::nullopt,
          absl::nullopt,
          Action::BLOCK,
      },
      {
          R"({"filters": [{
            "rp_id": "foo.com",
            "action": "block",
          }]})",
          Operation::MAKE_CREDENTIAL,
          "example.com",
          absl::nullopt,
          absl::nullopt,
          Action::ALLOW,
      },
      {
          R"({"filters": [{
            "device": "usb-1234:4321",
            "action": "block",
          }]})",
          Operation::MAKE_CREDENTIAL,
          "example.com",
          "usb-1234:4321",
          absl::nullopt,
          Action::BLOCK,
      },
      {
          R"({"filters": [{
            "device": "usb-1234:4321",
            "action": "block",
          }]})",
          Operation::MAKE_CREDENTIAL,
          "example.com",
          "usb-0000:4321",
          absl::nullopt,
          Action::ALLOW,
      },
      {
          R"({"filters": [{
            "device": "usb-*",
            "action": "block",
          }]})",
          Operation::MAKE_CREDENTIAL,
          "example.com",
          "usb-0000:4321",
          absl::nullopt,
          Action::BLOCK,
      },
      {
          R"({"filters": [{
            "device": "usb-*",
            "action": "block",
          }]})",
          Operation::MAKE_CREDENTIAL,
          "example.com",
          "usb-0000:4321",
          absl::nullopt,
          Action::BLOCK,
      },
      {
          R"({"filters": [{
            "id_type": "cred",
            "rp_id": "*",
            "action": "block",
          }]})",
          Operation::MAKE_CREDENTIAL,
          "example.com",
          absl::nullopt,
          cred_id,
          Action::BLOCK,
      },
      {
          R"({"filters": [{
            "id_type": "user",
            "rp_id": "*",
            "action": "block",
          }]})",
          Operation::MAKE_CREDENTIAL,
          "example.com",
          absl::nullopt,
          cred_id,
          Action::ALLOW,
      },
      {
          R"({"filters": [{
            "id_type": "cred",
            "id": "01*",
            "rp_id": "*",
            "action": "block",
          }]})",
          Operation::MAKE_CREDENTIAL,
          "example.com",
          absl::nullopt,
          cred_id,
          Action::BLOCK,
      },
      {
          R"({"filters": [{
            "id_type": "cred",
            "id": "01020304",
            "rp_id": "*",
            "action": "block",
          }]})",
          Operation::MAKE_CREDENTIAL,
          "example.com",
          absl::nullopt,
          cred_id,
          Action::BLOCK,
      },
      {
          R"({"filters": [{
            "id_type": "cred",
            "id_min_size": 4,
            "rp_id": "*",
            "action": "block",
          }]})",
          Operation::MAKE_CREDENTIAL,
          "example.com",
          absl::nullopt,
          cred_id,
          Action::BLOCK,
      },
      {
          R"({"filters": [{
            "id_type": "cred",
            "id_min_size": 8,
            "rp_id": "*",
            "action": "block",
          }]})",
          Operation::MAKE_CREDENTIAL,
          "example.com",
          absl::nullopt,
          cred_id,
          Action::ALLOW,
      },
      {
          R"({"filters": [{
            "id_type": "cred",
            "id_min_size": 4,
            "id_max_size": 4,
            "rp_id": "*",
            "action": "block",
          }]})",
          Operation::MAKE_CREDENTIAL,
          "example.com",
          absl::nullopt,
          cred_id,
          Action::BLOCK,
      },
      // id fields can be empty, to match an empty ID.
      {
          R"({"filters": [{
            "operation": "mc",
            "rp_id": "*",
            "id_type": "cred",
            "id": "",
            "action": "block",
          }]})",
          Operation::MAKE_CREDENTIAL,
          "example.com",
          absl::nullopt,
          empty_cred_id,
          Action::BLOCK,
      },
      // rp_id can be a list of strings, any of which may match.
      {
          R"({"filters": [{
            "operation": "mc",
            "rp_id": ["a.com", "b.com"],
            "action": "block",
          }]})",
          Operation::MAKE_CREDENTIAL,
          "a.com",
          absl::nullopt,
          absl::nullopt,
          Action::BLOCK,
      },
      {
          R"({"filters": [{
            "operation": "mc",
            "rp_id": ["a.com", "b.com"],
            "action": "block",
          }]})",
          Operation::MAKE_CREDENTIAL,
          "b.com",
          absl::nullopt,
          absl::nullopt,
          Action::BLOCK,
      },
      {
          R"({"filters": [{
            "operation": "mc",
            "rp_id": ["a.com", "b.com"],
            "action": "block",
          }]})",
          Operation::MAKE_CREDENTIAL,
          "c.com",
          absl::nullopt,
          absl::nullopt,
          Action::ALLOW,
      },
      // id can be a list of strings, any of which may match.
      {
          R"({"filters": [{
            "operation": "mc",
            "rp_id": "*",
            "id_type": "cred",
            "id": ["01", "01020304", "05060708", ""],
            "action": "block",
          }]})",
          Operation::MAKE_CREDENTIAL,
          "a.com",
          absl::nullopt,
          cred_id,
          Action::BLOCK,
      },
      // The two following cases test that a more specific filter can permit
      // something blocked by a later, more general, filter.
      {
          R"({"filters": [{
            "rp_id": "*",
            "device": "usb-1234:1234",
            "action": "allow",
          },{
            "rp_id": "*",
            "device": "usb-1234:*",
            "action": "block",
          }]})",
          Operation::MAKE_CREDENTIAL,
          "example.com",
          "usb-1234:5678",
          absl::nullopt,
          Action::BLOCK,
      },
      {
          R"({"filters": [{
            "rp_id": "*",
            "device": "usb-1234:1234",
            "action": "allow",
          },{
            "rp_id": "*",
            "device": "usb-1234:*",
            "action": "block",
          }]})",
          Operation::MAKE_CREDENTIAL,
          "example.com",
          "usb-1234:1234",
          absl::nullopt,
          Action::ALLOW,
      },
  };

  int test_num = 0;
  for (const auto& test : kTests) {
    SCOPED_TRACE(test_num);
    SCOPED_TRACE(test.filter);
    test_num++;
    ScopedFilterForTesting filter(test.filter);

    const Action result = Evaluate(test.op, test.rp_id, test.device, test.id);
    EXPECT_EQ(result, test.expected);
  }
}

TEST(FidoFilter, InvalidFilters) {
  static const char* kTests[] = {
      // Value has the wrong type.
      R"({"filters": [{
        "operation": 8,
        "rp_id": "foo.com",
        "action": "block"
      }]})",
      // Missing action.
      R"({"filters": [{
        "id_type": "cred",
        "id_min_size": 8
      }]})",
      // Unknown action.
      R"({"filters": [{
        "id_type": "cred",
        "id_min_size": 8,
        "action": "foo"
      }]})",
      // Missing ID types
      R"({"filters": [{
        "id_min_size": 8,
        "action": "block"
      }]})",
      R"({"filters": [{
        "id_max_size": 8,
        "action": "block"
      }]})",
      R"({"filters": [{
        "id": "112233",
        "action": "block"
      }]})",
      // Can't match everything.
      R"({"filters": [{
        "operation": "mc",
        "action": "block"
      }]})",
      // Can't match everything.
      R"({"filters": [{
        "operation": "ga",
        "action": "block"
      }]})",
      // Unknown keys are an error
      R"({"filters": [{
        "operation": "ga",
        "rp_id": "foo.com",
        "action": "block",
        "unknown": "bar"
      }]})",
      // No string values except IDs can be empty.
      R"({"filters": [{
        "operation": "ga",
        "rp_id": "",
        "action": "block",
      }]})",
      R"({"filters": [{
        "operation": "ga",
        "rp_id": ["nonempty", ""],
        "action": "block",
      }]})",
      R"({"filters": [{
        "operation": "ga",
        "rp_id": [],
        "action": "block",
      }]})",
      R"({"filters": [{
        "operation": "ga",
        "rp_id": "*",
        "id": [],
        "action": "block",
      }]})",
  };

  int test_num = 0;
  for (const auto* test : kTests) {
    SCOPED_TRACE(test_num);
    SCOPED_TRACE(test);
    test_num++;

    EXPECT_FALSE(ParseForTesting(test));
  }
}

TEST(FidoFilter, InvalidJSON) {
  // Testing that nothing crashes, etc.
  ScopedFilterForTesting filter(
      "nonsense", ScopedFilterForTesting::PermitInvalidJSON::kYes);
  ASSERT_EQ(Evaluate(Operation::GET_ASSERTION, "example.com", absl::nullopt,
                     absl::nullopt),
            Action::ALLOW);
  ASSERT_EQ(Evaluate(Operation::MAKE_CREDENTIAL, "example.com", absl::nullopt,
                     absl::nullopt),
            Action::ALLOW);
}

}  // namespace
}  // namespace fido_filter
}  // namespace device
