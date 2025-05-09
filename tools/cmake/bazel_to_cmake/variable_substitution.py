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
"""Implements $(location) and Make variable substitution."""

# pylint: disable=relative-beyond-top-level,invalid-name

from collections.abc import Callable
import io
import json
import os
import pathlib
import re
import shlex
from typing import Dict, List, Match, Optional

from .cmake_provider import CMakeExecutableTargetProvider
from .cmake_provider import CMakeLinkLibrariesProvider
from .cmake_target import CMakeTarget
from .evaluation import EvaluationState
from .starlark.bazel_target import TargetId
from .starlark.common_providers import FilesProvider
from .starlark.invocation_context import InvocationContext
from .starlark.toolchain import get_toolchain_substitutions
from .starlark.toolchain import MakeVariableSubstitutions
from .util import make_relative_path

_LOCATION_RE = re.compile(
    r"^(location|locations|execpath|execpaths|rootpath|rootpaths)\s+(.*)$"
)

_LOCATION_SUB_RE = re.compile(
    r"\$\((location|locations|execpath|execpaths|rootpath|rootpaths)\s+([^)]+)\)"
)


def _do_variable_replacement(
    cmd: str, get_replacement: Callable[[str, str], str]
) -> str:
  """Applies variable replacement to a string."""

  # NOTE: location and make variable substitutions do not compose well since
  # for location substitutions to work correctly CMake generator expressions
  # are needed.
  def _do_replace_impl(_cmd):
    i = _cmd.find("$")
    if i == -1:
      return _cmd, None

    j = i + 1
    if _cmd[j] == "$":
      return _cmd[:j], _cmd[j + 1 :]

    if _cmd[j] == "(":
      closeparen = ")"
    elif _cmd[j] == "{":
      closeparen = "}"
    else:
      # Single character literal.
      r = get_replacement("", _cmd[j])
      return f"{_cmd[:i]}{r}", _cmd[j + 1 :]

    # Find matching close, counting the nesting parens.
    k = j + 1
    count = 1
    while k < len(_cmd):
      if _cmd[k] == _cmd[j]:
        count += 1
      elif _cmd[k] == closeparen:
        count -= 1
        if count == 0:
          break
      k += 1

    # Do replacements on the sub-string.
    a, b = _do_replace_impl(_cmd[j + 1 : k])
    if b is None:
      b = ""

    r = get_replacement(_cmd[j], a + b)
    return f"{_cmd[:i]}{r}", _cmd[k + 1 :]

  out = io.StringIO()
  b = cmd
  while b:
    a, b = _do_replace_impl(b)
    out.write(a)
  return out.getvalue()


def _get_location_replacement(
    _context: InvocationContext,
    _cmd: str,
    relative_to: str,
    add_dependencies: Optional[List[CMakeTarget]],
    key: str,
    label: str,
) -> str:
  """Returns a $(location) replacement for the given key and label."""

  target = _context.resolve_target(label)
  state = _context.access(EvaluationState)

  # First-party references must exist.
  if _context.caller_package_id.repository_id == target.repository_id:
    info = state.get_target_info(target)
  else:
    info = state.get_optional_target_info(target)

  if not info:
    # This target is not available; construct an ephemeral reference.
    cmake_target = state.generate_cmake_target_pair(target)
    if add_dependencies is not None:
      add_dependencies.append(cmake_target.dep)
    return f"$<TARGET_FILE:{cmake_target.target}>"

  files_provider = info.get(FilesProvider)
  if files_provider is not None:
    if relative_to is not None:
      rel_paths = [
          make_relative_path(path, (None, relative_to))[1].as_posix()
          for path in files_provider.paths
      ]
    else:
      rel_paths = [str(path) for path in files_provider.paths]
    if not key.endswith("s"):
      if len(rel_paths) != 1:
        raise ValueError(f"Expected single file but received: {rel_paths}")
      return rel_paths[0]
    return " ".join(rel_paths)

  cmake_target_provider = info.get(CMakeExecutableTargetProvider)
  if cmake_target_provider is None:
    cmake_target_provider = info.get(CMakeLinkLibrariesProvider)
  if cmake_target_provider is not None:
    return f"$<TARGET_FILE:{cmake_target_provider.target}>"

  raise ValueError(
      f'Make location replacement failed: "{json.dumps(_cmd)}" with '
      f"key={key}, target={target.as_label()}, TargetInfo={repr(info)}"
  )


def _apply_location_and_make_variable_substitutions(
    _context: InvocationContext,
    *,
    cmd: str,
    relative_to: str,
    add_dependencies: Optional[List[CMakeTarget]],
    substitutions: MakeVariableSubstitutions,
    toolchains: Optional[List[TargetId]],
    enable_location: bool,
) -> str:
  """Applies $(location) and Bazel Make variable substitutions."""
  if toolchains is None:
    toolchains = []

  substitutions = get_toolchain_substitutions(
      _context, toolchains, substitutions
  )

  # NOTE: location and make variable substitutions do not compose well since
  # for location substitutions to work correctly CMake generator expressions
  # are needed.
  def _get_replacement(paren: str, name: str):
    if paren == "{":
      return ""  # Not really supported.

    if paren == "(" and enable_location:
      m = _LOCATION_RE.fullmatch(name)
      if m:
        return _get_location_replacement(
            _context,
            cmd,
            relative_to,
            add_dependencies,
            m.group(1),
            m.group(2),
        )

    replacement = substitutions.get(name)
    if replacement is None:
      raise ValueError(
          f"Undefined make variable: '{name}' in {json.dumps(cmd)} with"
          f" {substitutions}"
      )
    return replacement

  return _do_variable_replacement(cmd, _get_replacement)


def apply_make_variable_substitutions(
    _context: InvocationContext,
    cmd: str,
    substitutions: MakeVariableSubstitutions,
    toolchains: Optional[List[TargetId]] = None,
) -> str:
  """Applies Bazel Make variable substitutions.

  Args:
    _context: Context for resolving toolchain substitutions.
    cmd: Input string.
    substitutions: Substitutions to apply.
    toolchains: Toolchains defining additional substitutions.

  Returns:
    Substituted string.
  """
  return _apply_location_and_make_variable_substitutions(
      _context,
      cmd=cmd,
      relative_to="",
      add_dependencies=None,
      substitutions=substitutions,
      toolchains=toolchains,
      enable_location=False,
  )


def apply_location_and_make_variable_substitutions(
    _context: InvocationContext,
    *,
    cmd: str,
    relative_to: str,
    add_dependencies: Optional[List[CMakeTarget]],
    substitutions: MakeVariableSubstitutions,
    toolchains: Optional[List[TargetId]],
) -> str:
  """Applies $(location) and Bazel Make variable substitutions."""
  return _apply_location_and_make_variable_substitutions(
      _context,
      cmd=cmd,
      relative_to=relative_to,
      add_dependencies=add_dependencies,
      substitutions=substitutions,
      toolchains=toolchains,
      enable_location=True,
  )


def apply_location_substitutions(
    _context: InvocationContext,
    cmd: str,
    relative_to: str,
    add_dependencies: Optional[List[CMakeTarget]] = None,
) -> str:
  """Substitutes $(location) references in `cmd`.

  https://bazel.build/reference/be/make-variables#predefined_label_variables

  Args:
    _context: InvocationContext used for label resolution.
    cmd: Source string.
    relative_to: Working directory.
    add_dependencies: cmake target dependencies for the genrule

  Returns:
    Modified string.
  """

  def _replace(m: Match[str]) -> str:
    return _get_location_replacement(
        _context, cmd, relative_to, add_dependencies, m.group(1), m.group(2)
    )

  return _LOCATION_SUB_RE.sub(_replace, cmd)


def generate_substitutions(
    _context: InvocationContext,
    _target: TargetId,
    *,
    src_files: List[str],
    out_files: List[str],
) -> Dict[str, str]:
  # https://bazel.build/reference/be/make-variables
  # https://github.com/bazelbuild/examples/tree/main/make-variables#predefined-path-variables
  #
  # $(BINDIR): bazel-out/x86-fastbuild/bin
  # $(GENDIR): bazel-out/x86-fastbuild/bin
  # $(RULEDIR): bazel-out/x86-fastbuild/bin/testapp
  #
  # Multiple srcs, outs:
  #   $(SRCS): testapp/show_genrule_variables1.src testapp/...
  #   $(OUTS): bazel-out/x86-fastbuild/bin/testapp/subdir/show_genrule_variables1.out bazel-out/...
  #   $(@D): bazel-out/x86-fastbuild/bin/testapp
  #
  # Single srcs, outs
  #   $<: testapp/show_genrule_variables1.src
  #   $@: bazel-out/x86-fastbuild/bin/testapp/subdir/single_file_genrule.out
  #   $(@D): bazel-out/x86-fastbuild/bin/testapp/subdir
  #
  # TODO(jbms): Add missing variables, including:
  #   "$(COMPILATION_MODE)"
  #   "$(TARGET_CPU)"

  source_directory = _context.resolve_source_root(
      _context.caller_package_id.repository_id
  )
  binary_directory = pathlib.PurePosixPath(
      _context.resolve_output_root(_target.repository_id)
  )

  relative_src_paths = [
      (make_relative_path(s, (None, source_directory))[1]).as_posix()
      for s in src_files
  ]

  package_binary_dir = binary_directory.joinpath(_target.package_name)
  quoted_out_files = [json.dumps(path) for path in out_files]

  substitutions: Dict[str, str] = {
      "GENDIR": str(binary_directory),
      "BINDIR": str(binary_directory),
      "SRCS": " ".join([json.dumps(p) for p in relative_src_paths]),
      "OUTS": " ".join(quoted_out_files),
      "RULEDIR": str(package_binary_dir),
      "@D": str(package_binary_dir),
  }

  if len(src_files) == 1:
    substitutions["<"] = json.dumps(relative_src_paths[0])

  if len(out_files) == 1:
    substitutions["@"] = quoted_out_files[0]
    substitutions["@D"] = json.dumps(os.path.dirname(out_files[0]))

  return substitutions


def do_bash_command_replacement(cmd: str) -> str:
  """Tries to apply some bash-equivalent commands to a string."""

  # mimic shell $(dirname x)
  def _dirname(args: List[str]) -> str:
    if not args:
      raise ValueError(f"cannot apply `dirname` in {cmd}")
    dirnames = [os.path.dirname(x) for x in args]
    return "\n".join([x if x else "." for x in dirnames])

  # NOTE: location and make variable substitutions do not compose well since
  # for location substitutions to work correctly CMake generator expressions
  # are needed.
  def _get_replacement(paren: str, name: str):

    if not paren:
      return "$" + name

    if paren == "{":
      return "${" + name + "}"

    bash_command = shlex.split(name.strip())

    if bash_command[0].lower() == "dirname":
      return _dirname(bash_command[1:])

    return "$(" + name + ")"

  return _do_variable_replacement(cmd, _get_replacement)
