load("@rules_cc//cc:defs.bzl", "cc_proto_library")

cc_library(
    name = "crc32",
    srcs = ["crc32.cc"],
    hdrs = ["crc32.h"],
    visibility = ["//visibility:private"],
    deps = [
        "@com_google_absl//absl/strings",
        "@zlib",
    ],
)

cc_test(
    name = "crc32_test",
    srcs = ["crc32_test.cc"],
    visibility = ["//visibility:private"],
    deps = [
        ":crc32",
        "@googletest//:gtest_main",
        "@zlib",
    ],
)

cc_library(
    name = "file-storage",
    srcs = ["file-storage.cc"],
    hdrs = ["file-storage.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "file-storage_test",
    srcs = [
        "file-storage_test.cc",
        "testfile-fixture.h",
    ],
    visibility = ["//visibility:private"],
    deps = [
        ":file-storage",
        ":testing-matchers",
        "@com_google_absl//absl/strings",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "proto-data-store",
    srcs = [
        "proto-data-store.h",
        "status-macros.h",
    ],
    hdrs = ["proto-data-store.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":crc32",
        ":file-storage",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_test(
    name = "proto-data-store_test",
    srcs = [
        "proto-data-store_test.cc",
        "testfile-fixture.h",
    ],
    visibility = ["//visibility:private"],
    deps = [
        ":file-storage",
        ":proto-data-store",
        ":test_cc_proto",
        ":testing-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "testing-matchers",
    srcs = ["testing-matchers.h"],
    visibility = ["//visibility:private"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_proto_library(
    name = "test_cc_proto",
    visibility = ["//visibility:private"],
    deps = [":test_proto"],
)

proto_library(
    name = "test_proto",
    srcs = ["test.proto"],
    visibility = ["//visibility:private"],
)
