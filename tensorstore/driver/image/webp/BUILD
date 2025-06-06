load("//bazel:constants.bzl", "NO_STRINGOP_OVERLOAD")
load("//bazel:tensorstore.bzl", "tensorstore_cc_library")
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
    name = "webp",
    srcs = ["driver.cc"],
    copts = NO_STRINGOP_OVERLOAD,
    deps = [
        "//tensorstore:array",
        "//tensorstore:data_type",
        "//tensorstore:index",
        "//tensorstore/driver",
        "//tensorstore/driver/image:driver_impl",
        "//tensorstore/internal/image",
        "//tensorstore/internal/image:webp",
        "//tensorstore/internal/json_binding",
        "//tensorstore/kvstore:auto_detect",
        "//tensorstore/util:result",
        "//tensorstore/util:span",
        "//tensorstore/util:status",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings:cord",
        "@riegeli//riegeli/bytes:cord_reader",
        "@riegeli//riegeli/bytes:cord_writer",
    ],
    alwayslink = True,
)
