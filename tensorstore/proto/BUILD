load(
    "//bazel:tensorstore.bzl",
    "tensorstore_cc_library",
    "tensorstore_cc_proto_library",
    "tensorstore_cc_test",
    "tensorstore_proto_library",
)

package(default_visibility = ["//tensorstore:internal_packages"])

licenses(["notice"])

tensorstore_proto_library(
    name = "array_proto",
    srcs = ["array.proto"],
)

tensorstore_cc_proto_library(
    name = "array_cc_proto",
    deps = [":array_proto"],
)

tensorstore_proto_library(
    name = "index_transform_proto",
    srcs = ["index_transform.proto"],
)

tensorstore_cc_proto_library(
    name = "index_transform_cc_proto",
    deps = [":index_transform_proto"],
)

tensorstore_proto_library(
    name = "schema_proto",
    srcs = ["schema.proto"],
    deps = [
        ":array_proto",
        ":index_transform_proto",
    ],
)

tensorstore_cc_proto_library(
    name = "schema_cc_proto",
    deps = [":schema_proto"],
)

tensorstore_cc_library(
    name = "array",
    srcs = ["array.cc"],
    hdrs = ["array.h"],
    deps = [
        ":array_cc_proto",
        "//tensorstore:array",
        "//tensorstore:contiguous_layout",
        "//tensorstore:data_type",
        "//tensorstore:index",
        "//tensorstore:index_interval",
        "//tensorstore:rank",
        "//tensorstore:strided_layout",
        "//tensorstore/internal:elementwise_function",
        "//tensorstore/internal:integer_overflow",
        "//tensorstore/internal:unaligned_data_type_functions",
        "//tensorstore/util:dimension_set",
        "//tensorstore/util:iterate",
        "//tensorstore/util:result",
        "//tensorstore/util:str_cat",
        "@abseil-cpp//absl/status",
        "@riegeli//riegeli/bytes:string_reader",
        "@riegeli//riegeli/bytes:string_writer",
    ],
)

tensorstore_cc_test(
    name = "array_test",
    size = "small",
    srcs = ["array_test.cc"],
    deps = [
        ":array",
        ":array_cc_proto",
        ":protobuf_matchers",
        "//tensorstore:array",
        "//tensorstore:contiguous_layout",
        "//tensorstore:data_type",
        "//tensorstore:index",
        "//tensorstore:strided_layout",
        "//tensorstore/index_space:index_transform_testutil",
        "//tensorstore/internal:data_type_random_generator",
        "//tensorstore/internal/testing:random_seed",
        "//tensorstore/util:status_testutil",
        "@abseil-cpp//absl/status",
        "@googletest//:gtest_main",
    ],
)

tensorstore_cc_library(
    name = "encode_time",
    srcs = ["encode_time.cc"],
    hdrs = ["encode_time.h"],
    deps = [
        "//tensorstore/util:result",
        "//tensorstore/util:str_cat",
        "@abseil-cpp//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

tensorstore_cc_test(
    name = "encode_time_test",
    srcs = ["encode_time_test.cc"],
    deps = [
        ":encode_time",
        "//tensorstore/util:status_testutil",
        "//tensorstore/util:str_cat",
        "@com_google_protobuf//:protobuf",
        "@googletest//:gtest_main",
    ],
)

tensorstore_cc_library(
    name = "index_transform",
    srcs = ["index_transform.cc"],
    hdrs = ["index_transform.h"],
    deps = [
        ":index_transform_cc_proto",
        "//tensorstore:array",
        "//tensorstore:contiguous_layout",
        "//tensorstore:data_type",
        "//tensorstore:index",
        "//tensorstore:index_interval",
        "//tensorstore:json_serialization_options",
        "//tensorstore:rank",
        "//tensorstore/index_space:index_transform",
        "//tensorstore/index_space:output_index_method",
        "//tensorstore/internal/json:array",
        "//tensorstore/internal/json_binding",
        "//tensorstore/util:element_pointer",
        "//tensorstore/util:result",
        "//tensorstore/util:str_cat",
        "@abseil-cpp//absl/status",
    ],
)

tensorstore_cc_test(
    name = "index_transform_test",
    size = "small",
    srcs = ["index_transform_test.cc"],
    deps = [
        ":index_transform",
        ":index_transform_cc_proto",
        ":protobuf_matchers",
        "//tensorstore:array",
        "//tensorstore:box",
        "//tensorstore:index",
        "//tensorstore:index_interval",
        "//tensorstore:rank",
        "//tensorstore/index_space:dim_expression",
        "//tensorstore/index_space:index_transform",
        "//tensorstore/util:result",
        "//tensorstore/util:status_testutil",
        "@abseil-cpp//absl/status",
        "@googletest//:gtest_main",
    ],
)

tensorstore_cc_library(
    name = "schema",
    srcs = ["schema.cc"],
    hdrs = ["schema.h"],
    deps = [
        ":array",
        ":index_transform",
        ":schema_cc_proto",
        "//tensorstore:array",
        "//tensorstore:chunk_layout",
        "//tensorstore:codec_spec",
        "//tensorstore:data_type",
        "//tensorstore:index",
        "//tensorstore:rank",
        "//tensorstore:schema",
        "//tensorstore:strided_layout",
        "//tensorstore/index_space:dimension_units",
        "//tensorstore/index_space:index_transform",
        "//tensorstore/serialization",
        "//tensorstore/serialization:batch",
        "//tensorstore/util:dimension_set",
        "//tensorstore/util:result",
        "//tensorstore/util:span",
        "//tensorstore/util:status",
        "//tensorstore/util:unit",
        "@abseil-cpp//absl/status",
    ],
)

tensorstore_cc_test(
    name = "schema_test",
    size = "small",
    srcs = ["schema_test.cc"],
    deps = [
        ":protobuf_matchers",
        ":schema",
        ":schema_cc_proto",
        "//tensorstore:schema",
        "//tensorstore/util:result",
        "//tensorstore/util:status_testutil",
        "@abseil-cpp//absl/status",
        "@googletest//:gtest_main",
    ],
)

tensorstore_cc_library(
    name = "protobuf_matchers",
    testonly = 1,
    srcs = ["protobuf_matchers.cc"],
    hdrs = ["protobuf_matchers.h"],
    deps = [
        "@abseil-cpp//absl/log:absl_check",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/strings",
        "@com_google_protobuf//:protobuf",
        "@googletest//:gtest",
        "@re2",
    ],
)

tensorstore_cc_library(
    name = "proto_util",
    srcs = ["proto_util.cc"],
    hdrs = ["proto_util.h"],
    deps = [
        "//tensorstore/util:str_cat",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:str_format",
        "@com_google_protobuf//:protobuf",
    ],
)

tensorstore_cc_test(
    name = "proto_util_test",
    size = "small",
    srcs = ["proto_util_test.cc"],
    deps = [
        ":array_cc_proto",
        ":proto_util",
        ":protobuf_matchers",
        "@googletest//:gtest_main",
    ],
)

tensorstore_cc_library(
    name = "proto_binder",
    srcs = ["proto_binder.cc"],
    hdrs = ["proto_binder.h"],
    deps = [
        ":proto_util",
        "//tensorstore:json_serialization_options_base",
        "//tensorstore/internal/json:value_as",
        "//tensorstore/internal/json_binding",
        "//tensorstore/util:str_cat",
        "@abseil-cpp//absl/status",
        "@com_google_protobuf//:protobuf",
        "@nlohmann_json//:json",
    ],
)

tensorstore_cc_test(
    name = "proto_binder_test",
    size = "small",
    srcs = ["proto_binder_test.cc"],
    deps = [
        ":array_cc_proto",
        ":proto_binder",
        ":protobuf_matchers",
        "//tensorstore:json_serialization_options",
        "@abseil-cpp//absl/status",
        "@googletest//:gtest_main",
        "@nlohmann_json//:json",
    ],
)

tensorstore_cc_library(
    name = "parse_text_proto_or_die",
    hdrs = ["parse_text_proto_or_die.h"],
    deps = [
        ":proto_util",
        "//tensorstore/internal:source_location",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/strings",
    ],
)
