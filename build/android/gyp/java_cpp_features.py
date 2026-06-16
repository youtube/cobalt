#!/usr/bin/env python3
#
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import re
import sys
import zipfile

from util import build_utils
from util import java_cpp_utils
import action_helpers  # build_utils adds //build to sys.path.
import zip_helpers


class FeatureParserDelegate(java_cpp_utils.CppConstantParser.Delegate):
  # Ex. 'BASE_FEATURE(kConstantName, "StringNameOfTheFeature", ...);'
  # or 'STARBOARD_FEATURE(kConstantName, "StringNameOfTheFeature", ...);'
  # would parse as:
  #   ExtractConstantName() -> 'ConstantName'
  #   ExtractValue() -> '"StringNameOfTheFeature"'
  FEATURE_RE = re.compile(r'(?:BASE_FEATURE|STARBOARD_FEATURE)\(k([^,]+),')
  VALUE_RE = re.compile(r'\s*("(?:\"|[^"])*")\s*,')

  def ExtractConstantName(self, line):
    match = FeatureParserDelegate.FEATURE_RE.match(line)
    return match.group(1) if match else None

  def ExtractValue(self, line):
    match = FeatureParserDelegate.VALUE_RE.search(line)
    return match.group(1) if match else None

  def CreateJavaConstant(self, name, value, comments):
    return java_cpp_utils.JavaString(name, value, comments)


class ParameterParserDelegate(java_cpp_utils.CppConstantParser.Delegate):
  # Ex. 'STARBOARD_FEATURE_PARAM(kFeatureName, kConstantName, "StringName", ...)'
  # would parse as:
  #   ExtractConstantName() -> 'ConstantName'
  #   ExtractValue() -> '"StringName"'
  #   _feature_name -> 'FeatureName'
  PARAM_RE = re.compile(
      r'STARBOARD_FEATURE_PARAM\(\s*k([^,]+)\s*,\s*k([^,]+),')
  VALUE_RE = re.compile(r'\s*("(?:\"|[^"])*")\s*,')

  def __init__(self):
    self._feature_name = None

  def ExtractConstantName(self, line):
    match = ParameterParserDelegate.PARAM_RE.search(line)
    if not match:
      return None
    self._feature_name = match.group(1)
    return match.group(2)

  def ExtractValue(self, line):
    match = ParameterParserDelegate.VALUE_RE.search(line)
    return match.group(1) if match else None

  def CreateJavaConstant(self, name, value, comments):
    if self._feature_name:
      feature_name_sentence_case = java_cpp_utils.ToSentenceCase(
          self._feature_name)
      comments = (
          f'Parameter for the {feature_name_sentence_case} feature.',
      ) + comments
    return java_cpp_utils.JavaString(name, value, comments)



def _GenerateOutput(template, source_paths, template_path, features,
                    parameters):
  description_template = """
    // This following string constants were inserted by
    //     {SCRIPT_NAME}
    // From
    //     {SOURCE_PATHS}
    // Into
    //     {TEMPLATE_PATH}

"""
  values = {
      'SCRIPT_NAME': java_cpp_utils.GetScriptName(),
      'SOURCE_PATHS': ',\n    //     '.join(source_paths),
      'TEMPLATE_PATH': template_path,
  }
  description = description_template.format(**values)
  native_features = '\n\n'.join(x.Format() for x in features)
  native_parameters = '\n\n'.join(x.Format() for x in parameters)

  values = {
      'NATIVE_FEATURES': description + native_features,
      'NATIVE_PARAMETERS': description + native_parameters,
  }
  return template.format(**values)


def _ParseFeatureFile(path):
  with open(path) as f:
    feature_file_parser = java_cpp_utils.CppConstantParser(
        FeatureParserDelegate(), f.readlines())
  return feature_file_parser.Parse()


def _ParseParameterFile(path):
  with open(path) as f:
    parameter_file_parser = java_cpp_utils.CppConstantParser(
        ParameterParserDelegate(), f.readlines())
  return parameter_file_parser.Parse()


def _Generate(source_paths, template_path):
  with open(template_path) as f:
    lines = f.readlines()

  template = ''.join(lines)
  package, class_name = java_cpp_utils.ParseTemplateFile(lines)
  output_path = java_cpp_utils.GetJavaFilePath(package, class_name)

  features = []
  for source_path in source_paths:
    features.extend(_ParseFeatureFile(source_path))

  parameters = []
  for source_path in source_paths:
    parameters.extend(_ParseParameterFile(source_path))

  output = _GenerateOutput(template, source_paths, template_path, features,
                           parameters)
  return output, output_path


def _Main(argv):
  parser = argparse.ArgumentParser()

  parser.add_argument('--srcjar',
                      required=True,
                      help='The path at which to generate the .srcjar file')

  parser.add_argument('--template',
                      required=True,
                      help='The template file with which to generate the Java '
                      'class. Must have "{NATIVE_FEATURES}" somewhere in '
                      'the template.')

  parser.add_argument('inputs',
                      nargs='+',
                      help='Input file(s)',
                      metavar='INPUTFILE')
  args = parser.parse_args(argv)

  with action_helpers.atomic_output(args.srcjar) as f:
    with zipfile.ZipFile(f, 'w', zipfile.ZIP_STORED) as srcjar:
      data, path = _Generate(args.inputs, args.template)
      zip_helpers.add_to_zip_hermetic(srcjar, path, data=data)


if __name__ == '__main__':
  _Main(sys.argv[1:])