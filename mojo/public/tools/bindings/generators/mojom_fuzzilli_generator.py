# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse

import mojom.generate.generator as generator
import mojom.generate.module as mojom
from mojom.generate.template_expander import UseJinja
from generators.mojom_js_generator import JavaScriptStylizer

GENERATOR_PREFIX = "fuzzilli"
# Map primitive predicates to the fuzzilli type representation
PRIMITIVES_MAPPING = {
    mojom.BOOL: "boolean",
    mojom.INT8: "integer",
    mojom.INT16: "integer",
    mojom.INT32: "integer",
    mojom.INT64: "integer",
    mojom.UINT8: "integer",
    mojom.UINT16: "integer",
    mojom.UINT32: "integer",
    mojom.UINT64: "integer",
    mojom.FLOAT: "float",
    mojom.DOUBLE: "float",  # no dedicated `.double` type
    mojom.STRING: "string",
}

class Generator(generator.Generator):

  def __init__(self, *args, **kwargs):
    super(Generator, self).__init__(*args, **kwargs)
    self.primary_interface = None

  def GetFilters(self):
    return {
        "format_il_type": self._ILTypeName,
        "name_with_namespace": self._NameWithNamespace,
        "namespace_as_array": self._NamespaceAsArray,
        "to_camel": generator.ToCamel,
    }

  @staticmethod
  def GetTemplatePrefix():
    return "fuzzilli_templates"

  def _GetParameters(self):
    # Stylize first to get JS names (camelCase for fields/methods)
    self.module.Stylize(JavaScriptStylizer())

    return {"module": self.module, "primary": self.primary_interface}

  def _FuzzilliName(self, kind):
    name = []
    if kind.parent_kind:
      name.append(kind.parent_kind.name)
    name.append(kind.name)
    return "".join(name)

  # TODO(crbug.com/522372048): Handle nullable types explicitly. Currently, we
  # silently generate non-nullables for nullable types.
  def _ILTypeName(self, kind):
    if kind in PRIMITIVES_MAPPING:
      return PRIMITIVES_MAPPING[kind]

    if mojom.IsStructKind(kind) or mojom.IsEnumKind(kind):
      return f"js{self._FuzzilliName(kind)}"

    if mojom.IsInterfaceKind(kind):
      return f"js{self._FuzzilliName(kind)}Remote"

    if mojom.IsArrayKind(kind):
      return f"js{self._FuzzilliName(kind.kind)}Array"

    assert False, f"Unsupported type: {kind}."

  @UseJinja("fuzzilli_profile.tmpl")
  def _GenerateFuzzilliModule(self):
    return self._GetParameters()

  def GenerateFiles(self, unparsed_args):
    parser = argparse.ArgumentParser()
    parser.add_argument('--fuzzilli_primary_interface_name')
    args = parser.parse_args(unparsed_args)

    primary_interface_name = args.fuzzilli_primary_interface_name
    self.primary_interface = next((i for i in self.module.interfaces
                                   if i.mojom_name == primary_interface_name),
                                  None)
    if not self.primary_interface:
      raise Exception(
          f'Unable to find primary interface "{primary_interface_name}".')

    file_name = "%s.MojoProfile.swift" % self.module.path
    self.WriteWithComment(self._GenerateFuzzilliModule(), file_name)

  def _NameWithNamespace(self, kind):
    parent_suffix = kind.parent_kind.name if kind.parent_kind else ""
    return f"{kind.module.namespace}.{parent_suffix}{kind.name}"

  def _NamespaceAsArray(self, namespace):
    return namespace.split(".")
