# Framework for estimating heap memory usage of C++ types

load("//bazel:tensorstore.bzl", "tensorstore_cc_library", "tensorstore_cc_test")

package(default_visibility = ["//tensorstore:internal_packages"])

licenses(["notice"])

tensorstore_cc_library(
    name = "estimate_heap_usage",
    hdrs = [
        "estimate_heap_usage.h",
        "std_optional.h",
        "std_variant.h",
        "std_vector.h",
    ],
    deps = [
        "//tensorstore/util/apply_members",
        "@abseil-cpp//absl/strings:cord",
    ],
)

tensorstore_cc_test(
    name = "estimate_heap_usage_test",
    srcs = ["estimate_heap_usage_test.cc"],
    deps = [
        ":estimate_heap_usage",
        "//tensorstore/internal:intrusive_ptr",
        "//tensorstore/util/apply_members",
        "@abseil-cpp//absl/strings:cord",
        "@googletest//:gtest_main",
    ],
)
