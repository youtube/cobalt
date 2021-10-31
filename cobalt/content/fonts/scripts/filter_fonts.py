#!/usr/bin/python
# Copyright 2018 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Filters fonts.xml to include only desired font packages.

This is meant to be used in a GYP file to generate the list of fonts as the
input files for a target, as well as to re-write fonts.xml to include only
desired fonts.
"""

import argparse
from functools import partial
import sys
from xml.dom import Node
from xml.dom.minidom import parse as parse_xml

NORMAL_WEIGHT = 400
BOLD_WEIGHT = 700

NORMAL_STYLE = 'normal'
ITALIC_STYLE = 'italic'


def SelectFont(font_node, package_categories):
  """Filter function that keeps fonts according to the package font category.

  Args:
    font_node: The <font> node to evaluate.
    package_categories: Dictionary of package name to category number.
      The category values are described in cobalt/content/fonts/README.md
  Returns:
    True to keep this font, False to delete it.
  """
  weight = font_node.getAttribute('weight')
  if not weight:
    weight = NORMAL_WEIGHT
  else:
    weight = int(weight)

  style = font_node.getAttribute('style')
  if not style:
    style = NORMAL_STYLE

  package_name = GetContainerName('package', font_node)

  # Keep all fonts not in a named package.
  if not package_name:
    return True

  # When a package is excluded, keep fonts in the '!' version of the package.
  excluded = False
  if package_name[0] == '!':
    package_name = package_name[1:]
    excluded = True

  category = package_categories.get(package_name, '0')

  if excluded:
    return category == '0'
  elif category == '0':
    return False
  elif category == '1':
    return weight == NORMAL_WEIGHT and style == NORMAL_STYLE
  elif category == '2':
    return weight in (NORMAL_WEIGHT, BOLD_WEIGHT) and style == NORMAL_STYLE
  elif category == '3':
    return (weight in (NORMAL_WEIGHT, BOLD_WEIGHT) and
            style in (NORMAL_STYLE, ITALIC_STYLE))
  elif category == '4':
    return True
  else:
    raise ValueError(
        'Package category for "%s" must be between 0 and 4 (is: %s)' %
        (package_name, category))


def FilterFonts(filter_function, node, families, fonts):
  """Filter fonts in the document parsed from fonts.xml.

  Recursively traverses the document tree in a depth-first order, deleting
  unwanted nodes after visiting them.

  Args:
    filter_function: predicate method called for each font node.
    node: the root of the tree to evaluate recursively.
    families: list collected of remaining font families.
    fonts: list collected of remaining font files.
  Returns:
    True to keep this node, False to delete it.
  """
  # Depth-first traversal, deleting children that want to be deleted before
  # processing their parents. Make a new list of children so we can delete some
  # while iterating.
  for child in list(node.childNodes):
    if not FilterFonts(filter_function, child, families, fonts):
      node.removeChild(child).unlink()

  if node.nodeType == Node.TEXT_NODE:
    # Only keep text that has more than just whitespace.
    return node.data.strip()
  elif node.nodeType != Node.ELEMENT_NODE:
    # Only keep text and elements (no comments).
    return False

  # Only keep alias elements that refer to a family that was kept.
  if node.nodeName == 'alias':
    return node.getAttribute('to') in families

  # Delete nodes where all children were deleted.
  # We already kept relevant text and aliases, which have no children.
  if not node.childNodes:
    return False

  if node.nodeName == 'package':
    # Elevate all package children to here, and delete the package node.
    for child in list(node.childNodes):
      node.parentNode.insertBefore(node.removeChild(child), node)
    return False
  elif node.nodeName == 'family':
    # Remember the name of kept families (i.e. still have font children).
    families.append(node.getAttribute('name'))
  elif node.nodeName == 'font':
    # Only keep fonts that pass the filter_function.
    if filter_function(node):
      fonts.append(node.firstChild.data)
      return True
    else:
      return False

  return True


def GetContainerName(container_type, node):
  if node is None:
    return None
  elif node.nodeName == container_type:
    return node.getAttribute('name')
  else:
    return GetContainerName(container_type, node.parentNode)


def DoMain(argv):
  """Main function called from the command line.

  Args:
    argv: Command line parameters, without the program name.
  Returns:
    If --fonts_dir is specified, a newline-separated list of font files.
  """
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '-i',
      '--input_xml',
      required=True,
      help='path to fonts.xml to be filtered')
  parser.add_argument(
      '-o', '--output_xml', help='path to write a filtered XML file')
  parser.add_argument(
      '-f',
      '--fonts_dir',
      help='prints a list of font files prefixed by the '
      'specified FONTS_DIR (for GYP inputs)')
  parser.add_argument('package_categories', nargs=argparse.REMAINDER)
  options = parser.parse_args(argv)

  if not (options.output_xml or options.fonts_dir):
    parser.error('No output, specify output_xml, fonts_dir, or both')

  # Make a dictionary mapping package name to category.
  # E.g. ['sans-serif=1', 'serif=2'] becomes {'sans-serif':'1', 'serif':'2'}
  package_categories = dict(
      pkg.split('=') for pkg in options.package_categories)

  fonts_doc = parse_xml(options.input_xml)

  kept_families = []
  kept_fonts = []
  filter_function = partial(SelectFont, package_categories=package_categories)
  FilterFonts(filter_function, fonts_doc, kept_families, kept_fonts)

  if options.output_xml:
    with open(options.output_xml, 'w') as f:
      f.write(fonts_doc.toprettyxml(indent='  '))

  if options.fonts_dir:
    # Join with '/' rather than os.path.join() because this is for GYP, which
    # even on Windows wants slashes rather than backslashes.
    # Make a set for unique fonts since .ttc files may be listed more than once.
    result = [
        '/'.join((options.fonts_dir, font)) for font in sorted(set(kept_fonts))
    ]
    return '\n'.join(result)


def main(argv):
  result = DoMain(argv[1:])
  if result:
    print(result)
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
