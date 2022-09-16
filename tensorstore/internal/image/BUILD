load("//tensorstore:tensorstore.bzl", "tensorstore_cc_library", "tensorstore_cc_test")

package(
    default_visibility = ["//tensorstore:internal_packages"],
)

licenses(["notice"])

tensorstore_cc_library(
    name = "tiff",
    srcs = [
        "tiff_common.cc",
        "tiff_common.h",
        "tiff_reader.cc",
        "tiff_writer.cc",
    ],
    hdrs = [
        "tiff_reader.h",
        "tiff_writer.h",
    ],
    deps = [
        ":image",
        "//tensorstore:data_type",
        "//tensorstore/internal:logging",
        "//tensorstore/util:span",
        "//tensorstore/util:status",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_riegeli//riegeli/bytes:reader",
        "@com_google_riegeli//riegeli/bytes:writer",
        "@libtiff//:tiff",
    ],
)

tensorstore_cc_test(
    name = "tiff_test",
    srcs = ["tiff_test.cc"],
    args = [
        "--tensorstore_test_data_dir=" +
        package_name() + "/testdata",
    ],
    data = [":testdata"],
    deps = [
        ":riegeli_block_writer",
        ":tiff",
        "//tensorstore/internal:path",
        "//tensorstore/util:result",
        "//tensorstore/util:span",
        "//tensorstore/util:status_testutil",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:cord",
        "@com_google_googletest//:gtest_main",
        "@com_google_riegeli//riegeli/bytes:cord_reader",
        "@com_google_riegeli//riegeli/bytes:cord_writer",
        "@com_google_riegeli//riegeli/bytes:fd_reader",
        "@com_google_riegeli//riegeli/bytes:string_reader",
    ],
)

tensorstore_cc_library(
    name = "image",
    srcs = [
        "image_info.cc",
        "image_view.cc",
    ],
    hdrs = [
        "image_info.h",
        "image_reader.h",
        "image_view.h",
        "image_writer.h",
    ],
    deps = [
        "//tensorstore:data_type",
        "//tensorstore/util:result",
        "//tensorstore/util:span",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_riegeli//riegeli/bytes:reader",
        "@com_google_riegeli//riegeli/bytes:writer",
    ],
)

tensorstore_cc_test(
    name = "image_reader_test",
    srcs = ["image_reader_test.cc"],
    args = [
        "--tensorstore_test_data_dir=" +
        package_name() + "/testdata",
    ],
    data = [":testdata"],
    deps = [
        ":image",
        ":tiff",
        "//tensorstore/internal:path",
        "//tensorstore/util:result",
        "//tensorstore/util:status",
        "//tensorstore/util:status_testutil",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_google_riegeli//riegeli/base",
        "@com_google_riegeli//riegeli/bytes:cord_reader",
        "@com_google_riegeli//riegeli/bytes:fd_reader",
    ],
)

tensorstore_cc_test(
    name = "image_writer_test",
    srcs = ["image_writer_test.cc"],
    deps = [
        ":image",
        ":riegeli_block_writer",
        ":tiff",
        "//tensorstore/util:status_testutil",
        "@com_google_googletest//:gtest_main",
        "@com_google_riegeli//riegeli/base",
        "@com_google_riegeli//riegeli/bytes:cord_reader",
        "@com_google_riegeli//riegeli/bytes:cord_writer",
    ],
)

tensorstore_cc_library(
    name = "riegeli_block_writer",
    srcs = ["riegeli_block_writer.cc"],
    hdrs = ["riegeli_block_writer.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/types:optional",
        "@com_google_riegeli//riegeli/base",
        "@com_google_riegeli//riegeli/bytes:writer",
    ],
)

filegroup(
    name = "testdata",
    srcs = glob(
        include = [
            "testdata/**",
        ],
    ),
)