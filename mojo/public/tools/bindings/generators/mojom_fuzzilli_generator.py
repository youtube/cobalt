# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse

import mojom.generate.generator as generator
import mojom.generate.module as mojom
from mojom.generate.template_expander import UseJinja
from generators.mojom_js_generator import JavaScriptStylizer

GENERATOR_PREFIX = "fuzzilli"


class Generator(generator.Generator):

  def __init__(self, *args, **kwargs):
    super(Generator, self).__init__(*args, **kwargs)
    self.primary_interface = None

  def GetFilters(self):
    return {
        "to_camel": generator.ToCamel,
        "namespace_as_array": self._NamespaceAsArray,
    }

  @staticmethod
  def GetTemplatePrefix():
    return "fuzzilli_templates"

  def _GetParameters(self):
    # Stylize first to get JS names (camelCase for fields/methods)
    self.module.Stylize(JavaScriptStylizer())

    return {"module": self.module, "primary": self.primary_interface}

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

  def _NamespaceAsArray(self, namespace):
    return namespace.split(".")
