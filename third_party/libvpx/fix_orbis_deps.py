import re
import sys

abs_path_re = re.compile('[A-Za-z]:\\.*')

contents = sys.stdin.readlines()
if contents:
  contents = [line.rstrip() for line in contents]
  contents = [line.replace('"', '') for line in contents]
  _, first_dep_line = contents[0].split(':', 1)
  deps = []
  remaining_deps = [first_dep_line] + contents[1:]
  # Go through each line and split on whitespace.
  # There may be multiple deps on each line.
  for line in remaining_deps:
    line = line.strip(' \t\\')
    line = line.replace('\\', '/')
    line_deps = line.split()
    deps.extend(line_deps)

  # Strip out all absolute paths. Assume these are system includes
  # we don't care about. colons in the path confuse make.
  deps = filter(lambda d: not abs_path_re.match(d), deps)

  sys.stdout.write('%s %s: \\\n' % (deps[0] + '.d', deps[0] + '.o'))
  sys.stdout.write(' \\\n'.join([' %s' % d for d in deps]))
  sys.stdout.write('\n')
