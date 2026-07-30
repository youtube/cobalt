# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import enum
import hashlib
import os
import re
from typing import List, Dict, Set, Union

import gn_utils
import targets as gn2bp_targets

NEWLINE = ' " +\n         "'


class JniZeroTargetType(enum.Enum):
    GENERATOR = enum.auto()
    REGISTRATION_GENERATOR = enum.auto()


def get_jni_zero_target_type(target):
    if target.script != '//third_party/jni_zero/jni_zero.py':
        return None
    if target.common.args[0] == 'generate-final':
        return JniZeroTargetType.REGISTRATION_GENERATOR
    return JniZeroTargetType.GENERATOR


def write_blueprint_key_value(output,
                              name,
                              value,
                              sort=True,
                              list_to_multiline_string=False):
    """Writes a Blueprint key-value pair to the output.

  If list_to_multiline_string is set, and the value is a list, then the output
  value will be the list elements concatenated into a single Blueprint string,
  formatted such that each list element appears on its own line. This is a
  purely cosmetic feature to make the Blueprint file more readable.
  """

    def escape(s):
        return str(s).replace('\\', '\\\\').replace('"', '\\"')

    if isinstance(value, bool):
        if value:
            output.append('    %s: true,' % name)
        else:
            output.append('    %s: false,' % name)
        return
    if not value:
        return
    if isinstance(value, set):
        value = sorted(value)
    if isinstance(value, list) and not list_to_multiline_string:
        output.append('    %s: [' % name)
        for item in sorted(value) if sort else value:
            output.append('        "%s",' % escape(item))
        output.append('    ],')
        return
    if isinstance(value, Module.Target):
        value.to_string(output)
        return
    if isinstance(value, dict):
        kv_output = []
        for k, v in value.items():
            write_blueprint_key_value(kv_output, k, v)

        output.append('    %s: {' % name)
        for line in kv_output:
            output.append('    %s' % line)
        output.append('    },')
        return
    output.append(
        '    %s: "%s",' %
        (name,
         NEWLINE.join(
             escape(line)
             for line in (value if isinstance(value, list) else [value]))))


def label_to_module_name(label, context, short=False):
    """Turn a GN label (e.g., //:perfetto_tests) into a module name."""
    module = re.sub(r'^//:?', '', label)

    if short:
        # We want the module name to be short, but we still need it to be unique and
        # somewhat readable. To do this we replace just the path by a short hash.
        parts = module.rsplit('/', maxsplit=1)
        if len(parts) > 1 and len(parts[0]) > 10:
            module = hashlib.sha256(
                parts[0].encode('utf-8')).hexdigest()[:8] + '_' + parts[1]

    module = re.sub(r'[^a-zA-Z0-9_]', '_', module)

    if not module.startswith(context.module_prefix):
        return context.module_prefix + module
    return module


def get_protoc_module_name(gn, context):
    protoc_gn_target_name = gn.get_target(
        '//third_party/protobuf:protoc__toolchain_clang').name
    return label_to_module_name(protoc_gn_target_name, context)


class Module:
    """A single module (e.g., cc_binary, cc_test) in a blueprint."""

    class Target:
        """A target-scoped part of a module"""

        def __init__(self, name):
            self.name = name
            self.srcs = set()
            self.shared_libs = set()
            self.static_libs = set()
            self.whole_static_libs = set()
            self.header_libs = set()
            self.cflags = list()
            self.stl = None
            self.cppflags = list()
            self.include_dirs = set()
            self.generated_headers = set()
            self.export_generated_headers = set()
            self.ldflags = list()
            self.linker_scripts = set()
            self.compile_multilib = None
            self.stem = ""
            self.edition = ""
            self.features = set()
            self.cfgs = set()
            self.flags = list()
            self.rustlibs = set()
            self.proc_macros = set()

        def to_string(self, output):
            nested_out = []
            self._output_field(nested_out, 'srcs')
            self._output_field(nested_out, 'shared_libs')
            self._output_field(nested_out, 'static_libs')
            self._output_field(nested_out, 'whole_static_libs')
            self._output_field(nested_out, 'header_libs')
            # While sorting is a requirement for a deterministic output, sorting these flags correctly is
            # challenging. Sorting requires knowing the boundaries of each flag, but we cannot simply
            # assume flags are defined by whitespace, a leading - character, or something else.
            # With that in mind, we choose instead not to sort and instead we rely on GN's ordering of
            # these flags (and we assume that that ordering is deterministic).
            self._output_field(nested_out, 'cflags', sort=False)
            self._output_field(nested_out, 'stl')
            # The reasoning for disabling sort is the same as cflags.
            self._output_field(nested_out, 'cppflags', sort=False)
            self._output_field(nested_out, 'include_dirs')
            self._output_field(nested_out, 'generated_headers')
            self._output_field(nested_out, 'export_generated_headers')
            # The reasoning for disabling sort is the same as cflags.
            self._output_field(nested_out, 'ldflags', sort=False)
            self._output_field(nested_out, 'linker_scripts')
            self._output_field(nested_out, 'compile_multilib')
            self._output_field(nested_out, 'stem')
            self._output_field(nested_out, "edition")
            self._output_field(nested_out, 'cfgs')
            self._output_field(nested_out, 'features')
            self._output_field(nested_out, 'flags', sort=False)
            self._output_field(nested_out, 'rustlibs')
            self._output_field(nested_out, 'proc_macros')

            if nested_out:
                output.append('    %s: {' % self.name)
                for line in nested_out:
                    output.append('    %s' % line)
                output.append('    },')

        def _output_field(self,
                          output,
                          name,
                          sort=True,
                          list_to_multiline_string=False):
            return write_blueprint_key_value(
                output,
                name,
                getattr(self, name),
                sort=sort,
                list_to_multiline_string=list_to_multiline_string)

    def __init__(self, mod_type, name, gn_target, context):
        self.context = context
        self.type = mod_type
        self.gn_target = gn_target
        self.name = name
        self.srcs = set()
        self.comment = 'GN: ' + gn_target
        self.shared_libs = set()
        self.static_libs = set()
        self.whole_static_libs = set()
        self.tools = set()
        self.cmd = None
        self.host_supported = False
        self.host_cross_supported = True
        self.device_supported = True
        self.init_rc = set()
        self.out = set()
        self.export_include_dirs = set()
        self.generated_headers = set()
        self.export_generated_headers = set()
        self.export_static_lib_headers = set()
        self.export_header_lib_headers = set()
        self.defaults = set()
        self.cflags = list()
        self.include_dirs = set()
        self.local_include_dirs = set()
        self.header_libs = set()
        self.tool_files = set()
        # target contains a dict of Targets indexed by os_arch.
        # example: { 'android_x86': Target('android_x86')
        self.target = dict()
        self.target['android'] = self.Target('android')
        self.target['android_x86'] = self.Target('android_x86')
        self.target['android_x86_64'] = self.Target('android_x86_64')
        self.target['android_arm'] = self.Target('android_arm')
        self.target['android_arm64'] = self.Target('android_arm64')
        self.target['android_riscv64'] = self.Target('android_riscv64')
        self.target['host'] = self.Target('host')
        self.target['glibc'] = self.Target('glibc')
        self.stl = None
        self.cpp_std = None
        self.strip = dict()
        self.data = set()
        self.apex_available = set()
        self.min_sdk_version = None
        self.proto = dict()
        self.linker_scripts = set()
        self.ldflags = list()
        # The genrule_XXX below are properties that must to be propagated back
        # on the module(s) that depend on the genrule.
        self.genrule_headers = set()
        self.genrule_srcs = set()
        self.genrule_shared_libs = set()
        self.genrule_header_libs = set()
        self.version_script = None
        self.test_suites = set()
        self.test_config = None
        self.cppflags = list()
        self.rtti = False
        # Name of the output. Used for setting .so file name for libcronet
        self.libs = set()
        self.stem = None
        self.compile_multilib = None
        self.plugins = set()
        self.processor_class = None
        self.sdk_version = None
        self.javacflags = set()
        self.c_std = None
        self.default_applicable_licenses = set()
        self.default_visibility = []
        self.visibility = set()
        self.gn_type = None
        self.jarjar_rules = ""
        self.jars = set()
        self.build_file_path = None
        self.include_build_directory = None
        self.allow_rebasing = False
        self.license_kinds = set()
        self.license_text = set()
        self.errorprone = dict()
        self.crate_name = None
        # Should be arch-dependant
        self.crate_root = None
        self.edition = None
        self.rustlibs = set()
        self.proc_macros = set()
        self.wrapper_src = ""
        self.source_stem = ""
        self.bindgen_flags = set()
        self.handle_static_inline = None
        self.static_inline_library = ""
        self.jni_zero_target_type = None
        self.unstable = ""
        self.path = ""
        # In the case of Java "top-level" modules, this points to the corresponding
        # "unfiltered" module. The top-level module is just a dependency holder;
        # it's the unfiltered module that does the actual compiling. For more
        # details, see `create_java_module()`.
        self.java_unfiltered_module = None
        self.cargo_env_compat = None
        self.cargo_pkg_version = None
        self.whole_program_vtables = False
        self.afdo = None

    def variant(self, arch_name):
        return self if arch_name == 'common' else self.target[arch_name]

    def to_string(self, output):
        if self.comment:
            output.append('// %s' % self.comment)
        output.append('%s {' % self.type)
        self._output_field(output, 'name')
        self._output_field(output, 'srcs')
        self._output_field(output, 'shared_libs')
        self._output_field(output, 'static_libs')
        self._output_field(output, 'whole_static_libs')
        self._output_field(output, 'tools')
        self._output_field(output,
                           'cmd',
                           sort=False,
                           list_to_multiline_string=True)
        if self.host_supported:
            self._output_field(output, 'host_supported')
        if not self.host_cross_supported:
            self._output_field(output, 'host_cross_supported')
        if not self.device_supported:
            self._output_field(output, 'device_supported')
        self._output_field(output, 'init_rc')
        self._output_field(output, 'out')
        self._output_field(output, 'export_include_dirs')
        self._output_field(output, 'generated_headers')
        self._output_field(output, 'export_generated_headers')
        self._output_field(output, 'export_static_lib_headers')
        self._output_field(output, 'export_header_lib_headers')
        self._output_field(output, 'defaults')
        # While sorting is a requirement for a deterministic output, sorting these flags correctly is
        # challenging. Sorting requires knowing the boundaries of each flag, but we cannot simply
        # assume flags are defined by whitespace, a leading - character, or something else.
        # With that in mind, we choose instead not to sort and instead we rely on GN's ordering of
        # these flags (and we assume that that ordering is deterministic).
        self._output_field(output, 'cflags', sort=False)
        self._output_field(output, 'include_dirs')
        self._output_field(output, 'local_include_dirs')
        self._output_field(output, 'header_libs')
        self._output_field(output, 'strip')
        self._output_field(output, 'tool_files')
        self._output_field(output, 'data')
        self._output_field(output, 'stl')
        self._output_field(output, 'cpp_std')
        self._output_field(output, 'apex_available')
        self._output_field(output, 'min_sdk_version')
        self._output_field(output, 'version_script')
        self._output_field(output, 'test_suites')
        self._output_field(output, 'test_config')
        self._output_field(output, 'proto')
        self._output_field(output, 'linker_scripts')
        # The reasoning for disabling sort is the same as cflags.
        self._output_field(output, 'ldflags', sort=False)
        # The reasoning for disabling sort is the same as cflags.
        self._output_field(output, 'cppflags', sort=False)
        self._output_field(output, 'unstable')
        self._output_field(output, 'path')
        self._output_field(output, 'libs')
        self._output_field(output, 'stem')
        self._output_field(output, 'compile_multilib')
        self._output_field(output, 'plugins')
        self._output_field(output, 'processor_class')
        self._output_field(output, 'sdk_version')
        self._output_field(output, 'javacflags')
        self._output_field(output, 'c_std')
        self._output_field(output, 'default_applicable_licenses')
        self._output_field(output, 'default_visibility')
        self._output_field(output, 'visibility')
        self._output_field(output, 'jarjar_rules')
        self._output_field(output, 'jars')
        self._output_field(output, 'include_build_directory')
        self._output_field(output, 'license_text')
        self._output_field(output, "license_kinds")
        self._output_field(output, "errorprone")
        self._output_field(output, 'crate_name')
        self._output_field(output, 'crate_root')
        self._output_field(output, 'rustlibs')
        self._output_field(output, 'proc_macros')
        self._output_field(output, 'source_stem')
        self._output_field(output, 'bindgen_flags')
        self._output_field(output, 'wrapper_src')
        self._output_field(output, 'handle_static_inline')
        self._output_field(output, 'static_inline_library')
        self._output_field(output, 'cargo_env_compat')
        self._output_field(output, 'cargo_pkg_version')
        if self.whole_program_vtables:
            self._output_field(output, 'whole_program_vtables')
        if self.afdo:
            self._output_field(output, 'afdo')
        if self.rtti:
            self._output_field(output, 'rtti')
        target_out = []
        for arch, target in sorted(self.target.items()):
            # _output_field calls getattr(self, arch).
            setattr(self, arch, target)
            self._output_field(target_out, arch)

        if target_out:
            output.append('    target: {')
            for line in target_out:
                output.append('    %s' % line)
            output.append('    },')

        output.append('}')
        output.append('')

    def add_android_shared_lib(self, lib):
        if self.type.startswith('java'):
            raise Exception(
                'Adding Android shared lib for java_* targets is unsupported')
        if self.type == 'cc_binary_host':
            raise Exception(
                'Adding Android shared lib for host tool is unsupported')

        if self.host_supported:
            self.target['android'].shared_libs.add(lib)
        else:
            self.shared_libs.add(lib)

    def is_test(self):
        if gn_utils.TESTING_SUFFIX in self.name:
            name_without_prefix = self.name[:self.name.find(gn_utils.
                                                            TESTING_SUFFIX)]
            return any(name_without_prefix == label_to_module_name(
                target, self.context)
                       for target in gn2bp_targets.DEFAULT_TESTS)
        return False

    def _output_field(self,
                      output,
                      name,
                      sort=True,
                      list_to_multiline_string=False):
        return write_blueprint_key_value(
            output,
            name,
            getattr(self, name),
            sort=sort,
            list_to_multiline_string=list_to_multiline_string)

    def is_compiled(self):
        return self.type not in ('cc_genrule', 'filegroup', 'java_genrule')

    def is_genrule(self):
        return self.type == "cc_genrule"

    def has_input_files(self):
        if self.type in ["java_library", "java_import", "rust_bindgen"]:
            return True
        if len(self.srcs) > 0:
            return True
        if any(len(target.srcs) > 0 for target in self.target.values()):
            return True
        # Allow cc_static_library with export_generated_headers as those are crucial for
        # the depending modules
        return len(self.export_generated_headers) > 0 or len(
            self.generated_headers) > 0

    def is_java_top_level_module(self):
        return self.java_unfiltered_module is not None


class Blueprint:
    """In-memory representation of an Android.bp file."""

    def __init__(self, buildgn_directory_path: str = ""):
        self.modules = {}
        # Holds the BUILD.gn path which resulted in the creation of this Android.bp.
        self._buildgn_directory_path = buildgn_directory_path
        self._readme_location = buildgn_directory_path
        self._package_module = None
        self._license_module = None

    def add_module(self, module):
        """Adds a new module to the blueprint, replacing any existing module
        with the same name.

        Args:
            module: Module instance.
        """
        self.modules[module.name] = module

    def set_package_module(self, module):
        self._package_module = module

    def set_license_module(self, module):
        self._license_module = module

    def get_license_module(self):
        return self._license_module

    def set_readme_location(self, readme_path: str):
        self._readme_location = readme_path

    def get_readme_location(self):
        return self._readme_location

    def get_buildgn_location(self):
        return self._buildgn_directory_path

    def to_string(self, exclude_gn_targets=None):
        ret = []
        if self._package_module:
            self._package_module.to_string(ret)
        if self._license_module:
            self._license_module.to_string(ret)
        for m in sorted(self.modules.values(), key=lambda m: m.name):
            if (m.type != "cc_library_static" or m.has_input_files()) and (
                    exclude_gn_targets is None
                    or m.gn_target not in exclude_gn_targets):
                # Don't print cc_library_static with empty srcs. These attributes are already
                # propagated up the tree. Printing them messes the presubmits because
                # every module is compiled while those targets are not reachable in
                # a normal compilation path.
                m.to_string(ret)
        return ret
