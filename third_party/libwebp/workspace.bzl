# Copyright 2022 The TensorStore Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# buildifier: disable=module-docstring

load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load(
    "//third_party:repo.bzl",
    "third_party_http_archive",
)

def repo():
    maybe(
        third_party_http_archive,
        name = "libwebp",
        doc_version = "1.5.0",
        urls = [
            "https://storage.googleapis.com/tensorstore-bazel-mirror/github.com/webmproject/libwebp/archive/v1.5.0.tar.gz",
            "https://github.com/webmproject/libwebp/archive/v1.5.0.tar.gz",
        ],
        sha256 = "668c9aba45565e24c27e17f7aaf7060a399f7f31dba6c97a044e1feacb930f37",
        strip_prefix = "libwebp-1.5.0",
        build_file = Label("//third_party:libwebp/libwebp.BUILD.bazel"),
        system_build_file = Label("//third_party:libwebp/system.BUILD.bazel"),
        cmake_name = "WebP",
        bazel_to_cmake = {},
        cmake_target_mapping = {
            "@libwebp//:webp": "WebP::webp",
            "@libwebp//:sharpyuv": "WebP::sharpyuv",
        },
        cmake_package_redirect_libraries = {
            "WebP": "WebP::webp",
            "LIBSHARPYUV": "WebP::sharpyuv",
        },
        cmake_package_aliases = ["libsharpyuv"],
    )
