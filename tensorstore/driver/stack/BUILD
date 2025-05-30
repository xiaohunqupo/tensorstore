load("//bazel:tensorstore.bzl", "tensorstore_cc_library", "tensorstore_cc_test")
load("//docs:doctest.bzl", "doctest_test")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

DOCTEST_SOURCES = glob([
    "**/*.rst",
    "**/*.yml",
])

doctest_test(
    name = "doctest_test",
    srcs = DOCTEST_SOURCES,
)

filegroup(
    name = "doc_sources",
    srcs = DOCTEST_SOURCES,
)

tensorstore_cc_library(
    name = "stack",
    srcs = [
        "driver.cc",
        "driver.h",
    ],
    hdrs = ["//tensorstore:stack.h"],
    deps = [
        "//tensorstore",
        "//tensorstore:box",
        "//tensorstore:context",
        "//tensorstore:data_type",
        "//tensorstore:index",
        "//tensorstore:index_interval",
        "//tensorstore:open_mode",
        "//tensorstore:open_options",
        "//tensorstore:rank",
        "//tensorstore:schema",
        "//tensorstore:spec",
        "//tensorstore:transaction",
        "//tensorstore/driver",
        "//tensorstore/driver:chunk",
        "//tensorstore/driver:chunk_receiver_utils",
        "//tensorstore/index_space:dim_expression",
        "//tensorstore/index_space:dimension_identifier",
        "//tensorstore/index_space:dimension_units",
        "//tensorstore/index_space:index_transform",
        "//tensorstore/internal:concurrency_resource",
        "//tensorstore/internal:context_binding",
        "//tensorstore/internal:data_copy_concurrency_resource",
        "//tensorstore/internal:grid_partition",
        "//tensorstore/internal:intrusive_ptr",
        "//tensorstore/internal:irregular_grid",
        "//tensorstore/internal:tagged_ptr",
        "//tensorstore/internal/cache:cache_pool_resource",
        "//tensorstore/internal/json_binding",
        "//tensorstore/internal/json_binding:staleness_bound",
        "//tensorstore/internal/meta:type_traits",
        "//tensorstore/serialization",
        "//tensorstore/util:dimension_set",
        "//tensorstore/util:executor",
        "//tensorstore/util:future",
        "//tensorstore/util:iterate_over_index_range",
        "//tensorstore/util:option",
        "//tensorstore/util:result",
        "//tensorstore/util:span",
        "//tensorstore/util:status",
        "//tensorstore/util:str_cat",
        "//tensorstore/util/execution:any_receiver",
        "//tensorstore/util/garbage_collection",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/hash",
        "@abseil-cpp//absl/log:absl_log",
        "@abseil-cpp//absl/meta:type_traits",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings:str_format",
    ],
    alwayslink = True,
)

tensorstore_cc_test(
    name = "driver_test",
    size = "small",
    srcs = ["driver_test.cc"],
    deps = [
        ":stack",
        "//tensorstore",
        "//tensorstore:array",
        "//tensorstore:array_testutil",
        "//tensorstore:box",
        "//tensorstore:context",
        "//tensorstore:data_type",
        "//tensorstore:index",
        "//tensorstore:open",
        "//tensorstore:open_mode",
        "//tensorstore:progress",
        "//tensorstore:schema",
        "//tensorstore:strided_layout",
        "//tensorstore:transaction",
        "//tensorstore/driver/array",
        "//tensorstore/driver/n5",
        "//tensorstore/driver/zarr3",
        "//tensorstore/index_space:dim_expression",
        "//tensorstore/index_space:index_transform",
        "//tensorstore/internal/testing:json_gtest",
        "//tensorstore/kvstore",
        "//tensorstore/kvstore/memory",
        "//tensorstore/util:result",
        "//tensorstore/util:span",
        "//tensorstore/util:status_testutil",
        "//tensorstore/util:unit",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest_main",
        "@nlohmann_json//:json",
    ],
)

tensorstore_cc_test(
    name = "integration_test",
    srcs = ["integration_test.cc"],
    deps = [
        ":stack",  # build_cleaner: keep
        "//tensorstore",
        "//tensorstore:array",
        "//tensorstore:array_testutil",
        "//tensorstore:box",
        "//tensorstore:context",
        "//tensorstore:data_type",
        "//tensorstore:index",
        "//tensorstore:open",
        "//tensorstore:open_mode",
        "//tensorstore:progress",
        "//tensorstore/driver/image/png",  # build_cleaner: keep
        "//tensorstore/driver/neuroglancer_precomputed",  # build_cleaner: keep
        "//tensorstore/index_space:dim_expression",
        "//tensorstore/kvstore",
        "//tensorstore/kvstore/memory",  # build_cleaner: keep
        "//tensorstore/util:result",
        "//tensorstore/util:status",
        "//tensorstore/util:status_testutil",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:cord",
        "@googletest//:gtest_main",
        "@nlohmann_json//:json",
    ],
)
