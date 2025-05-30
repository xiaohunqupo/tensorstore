load(
    "//bazel:tensorstore.bzl",
    "tensorstore_cc_library",
    "tensorstore_cc_test",
)

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

tensorstore_cc_library(
    name = "gcs_grpc",
    srcs = [
        "gcs_grpc.cc",
        "state.cc",
    ],
    hdrs = ["state.h"],
    deps = [
        ":default_endpoint",
        ":default_strategy",
        ":storage_stub_pool",
        "//tensorstore:context",
        "//tensorstore/internal:context_binding",
        "//tensorstore/internal:data_copy_concurrency_resource",
        "//tensorstore/internal:intrusive_ptr",
        "//tensorstore/internal:source_location",
        "//tensorstore/internal:uri_utils",
        "//tensorstore/internal/cache_key",
        "//tensorstore/internal/grpc:utils",
        "//tensorstore/internal/grpc/clientauth:authentication_strategy",
        "//tensorstore/internal/json_binding",
        "//tensorstore/internal/json_binding:absl_time",
        "//tensorstore/internal/json_binding:bindable",
        "//tensorstore/internal/log:verbose_flag",
        "//tensorstore/internal/metrics",
        "//tensorstore/internal/thread:schedule_at",
        "//tensorstore/kvstore",
        "//tensorstore/kvstore:batch_util",
        "//tensorstore/kvstore:common_metrics",
        "//tensorstore/kvstore:generation",
        "//tensorstore/kvstore:key_range",
        "//tensorstore/kvstore/gcs:exp_credentials_resource",
        "//tensorstore/kvstore/gcs:exp_credentials_spec",
        "//tensorstore/kvstore/gcs:gcs_resource",
        "//tensorstore/kvstore/gcs:validate",
        "//tensorstore/serialization",
        "//tensorstore/serialization:absl_time",
        "//tensorstore/util:executor",
        "//tensorstore/util:future",
        "//tensorstore/util:quote_string",
        "//tensorstore/util:result",
        "//tensorstore/util:status",
        "//tensorstore/util:str_cat",
        "//tensorstore/util/execution",
        "//tensorstore/util/execution:any_receiver",
        "//tensorstore/util/garbage_collection",
        "@abseil-cpp//absl/base:core_headers",
        "@abseil-cpp//absl/crc:crc32c",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings:cord",
        "@abseil-cpp//absl/strings:str_format",
        "@abseil-cpp//absl/synchronization",
        "@abseil-cpp//absl/time",
        "@com_google_protobuf//:protobuf",
        "@googleapis//google/storage/v2:storage_cc_grpc",
        "@googleapis//google/storage/v2:storage_cc_proto",
        "@grpc//:grpc++",
        "@riegeli//riegeli/bytes:cord_writer",
    ],
    alwayslink = 1,
)

tensorstore_cc_test(
    name = "gcs_grpc_test",
    srcs = ["gcs_grpc_test.cc"],
    tags = ["cpu:2"],
    deps = [
        ":gcs_grpc",
        ":mock_storage_service",
        "//tensorstore:context",
        "//tensorstore/internal:flat_cord_builder",
        "//tensorstore/internal/grpc:grpc_mock",
        "//tensorstore/internal/grpc:utils",
        "//tensorstore/kvstore",
        "//tensorstore/kvstore:byte_range",
        "//tensorstore/kvstore:generation",
        "//tensorstore/kvstore:key_range",
        "//tensorstore/kvstore:test_util",
        "//tensorstore/proto:parse_text_proto_or_die",
        "//tensorstore/proto:protobuf_matchers",
        "//tensorstore/util:future",
        "//tensorstore/util:span",
        "//tensorstore/util:status_testutil",
        "//tensorstore/util/execution",
        "//tensorstore/util/execution:sender_testutil",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings:cord",
        "@abseil-cpp//absl/strings:str_format",
        "@abseil-cpp//absl/synchronization",
        "@abseil-cpp//absl/time",
        "@googleapis//google/storage/v2:storage_cc_proto",
        "@googletest//:gtest_main",
        "@grpc//:grpc++",
    ],
)

tensorstore_cc_library(
    name = "mock_storage_service",
    testonly = 1,
    hdrs = ["mock_storage_service.h"],
    visibility = ["//visibility:private"],
    deps = [
        "//tensorstore/internal/grpc:grpc_mock",
        "@com_google_protobuf//:protobuf",
        "@googleapis//google/storage/v2:storage_cc_grpc",
        "@googleapis//google/storage/v2:storage_cc_proto",
        "@grpc//:grpc++",
    ],
)

tensorstore_cc_test(
    name = "gcs_grpc_testbench_test",
    size = "medium",
    srcs = [
        "gcs_grpc_testbench_test.cc",
    ],
    args = ["--testbench_binary=$(location @com_google_storagetestbench//:rest_server)"],
    data = ["@com_google_storagetestbench//:rest_server"],
    flaky = 1,
    tags = [
        "cpu:2",
        "requires-net:loopback",
        "skip-cmake",  # storage_testbench is python, which doesn't work in bazel_to_cmake yet.
        "skip-darwin",  # the python binary doesn't find dependencies on darwin.
    ],
    deps = [
        ":gcs_grpc",
        "//tensorstore/internal:global_initializer",
        "//tensorstore/internal/thread",
        "//tensorstore/kvstore",
        "//tensorstore/kvstore:batch_util",
        "//tensorstore/kvstore:generation",
        "//tensorstore/kvstore:test_util",
        "//tensorstore/kvstore/gcs:gcs_testbench",
        "//tensorstore/util:future",
        "//tensorstore/util:result",
        "//tensorstore/util:status_testutil",
        "@abseil-cpp//absl/base:no_destructor",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/log:absl_check",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:cord",
        "@googletest//:gtest_main",
    ],
)

tensorstore_cc_library(
    name = "storage_stub_pool",
    srcs = ["storage_stub_pool.cc"],
    hdrs = ["storage_stub_pool.h"],
    visibility = ["//visibility:private"],
    deps = [
        "//tensorstore/internal:env",
        "//tensorstore/internal/grpc/clientauth:authentication_strategy",
        "//tensorstore/internal/grpc/clientauth:create_channel",
        "//tensorstore/internal/log:verbose_flag",
        "@abseil-cpp//absl/base:core_headers",
        "@abseil-cpp//absl/base:no_destructor",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:str_format",
        "@abseil-cpp//absl/synchronization",
        "@abseil-cpp//absl/time",
        "@com_google_protobuf//:protobuf",
        "@googleapis//google/storage/v2:storage_cc_grpc",
        "@googleapis//google/storage/v2:storage_cc_proto",
        "@grpc//:grpc++",
        "@grpc//:grpc_security_base",
    ],
)

tensorstore_cc_library(
    name = "default_endpoint",
    srcs = ["default_endpoint.cc"],
    hdrs = ["default_endpoint.h"],
    deps = [
        "//tensorstore/internal:env",
        "//tensorstore/internal/os:file_util",
        "//tensorstore/internal/os:get_bios_info",
        "//tensorstore/util:result",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/strings",
    ],
)

DEFAULT_STRATEGY_DEPS = []

DEFAULT_STRATEGY_DEFINES = []

tensorstore_cc_library(
    name = "default_strategy",
    srcs = ["default_strategy.cc"],
    hdrs = ["default_strategy.h"],
    local_defines = DEFAULT_STRATEGY_DEFINES,
    deps = DEFAULT_STRATEGY_DEPS + [
        "//tensorstore/internal/grpc/clientauth:authentication_strategy",
        "//tensorstore/internal/grpc/clientauth:channel_authentication",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/strings",
    ],
)
