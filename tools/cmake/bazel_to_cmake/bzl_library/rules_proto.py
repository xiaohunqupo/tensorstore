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
"""CMake implementation of "@rules_proto".

See: https://github.com/bazelbuild/rules_proto/blob/master/proto/defs.bzl
"""

# pylint: disable=relative-beyond-top-level

from .. import native_rules_proto
from ..starlark.ignored import IgnoredObject
from ..starlark.scope_common import ScopeCommon
from .register import register_bzl_library


@register_bzl_library("@rules_proto//proto:defs.bzl")
class RulesProtoDefs(ScopeCommon):

  def bazel_proto_library(self, **kwargs):
    return native_rules_proto.proto_library(self._context, **kwargs)

  def bazel_proto_descriptor_set(self, **kwargs):
    del kwargs
    pass

  @property
  def bazel_proto_lang_toolchain(self):
    return IgnoredObject()

  def bazel_ProtoInfo(self, **kwargs):
    del kwargs
    return IgnoredObject()


@register_bzl_library("@rules_proto//proto:proto_toolchain.bzl")
class RulesProtoProtoToolchain(ScopeCommon):

  def bazel_proto_toolchain(self, **kwargs):
    del kwargs


@register_bzl_library("@rules_proto//proto:toolchains.bzl")
class RulesProtoToolchains(ScopeCommon):

  def bazel_proto_toolchains(self, **kwargs):
    del kwargs


@register_bzl_library("@rules_proto//proto:proto_lang_toolchain.bzl")
class RulesProtoProtoLangToolchain(ScopeCommon):

  def bazel_proto_lang_toolchain(self, **kwargs):
    del kwargs
