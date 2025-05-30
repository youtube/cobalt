import argparse
import os.path

_HEADER_TEMPLATE = """#ifndef {header_guard}
#define {header_guard}

#include {include}

{input}

#endif  // {header_guard}
"""

_SOURCE_TEMPLATE = """#include "{header_include}"

{input}

#include "{include}"
"""


def fill_tmpl(header, source, output_dir, for_starboard):
  include = 'starboard/feature_list.h' if for_starboard else 'base/feature_list.h'
  with open(header) as f:
    header_lines = f.read()
  with open(source) as f:
    source_lines = f.read()

  header_guard = os.path.basename(header).replace('.', '_').upper()
  header_content = _HEADER_TEMPLATE.format(
      header_guard=header_guard, include=include, input=header_lines)
  source_content = _SOURCE_TEMPLATE.format(
      header_include=header, include=include, input=source_lines)

  with open(os.path.join(output_dir, 'feature_config.h'), 'w') as f:
    f.write(header_content)
  with open(os.path.join(output_dir, 'feature_config.cc'), 'w') as f:
    f.write(source_content)


def main():
  parser = argparse.ArgumentParser(
      description='Generate feature header/source files.')
  parser.add_argument('--header', required=True)
  parser.add_argument('--source', required=True)
  parser.add_argument('--output-dir', required=True)
  parser.add_argument('--for-starboard', default=False, action='store_true')
  args = parser.parse_args()
  fill_tmpl(args.header, args.source, args.output_dir, args.for_starboard)


if __name__ == '__main__':
  main()
