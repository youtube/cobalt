#!/usr/bin/python
"""
%prog [options] inputfile outputfile

Packages inputfile and all of it's dependencies into a single python file
called outputfile
"""
import os
import imp


def find_module_path(module_name):
    bits = module_name.split(".")
    last_path = None
    for b in bits:
        fp, path, desc = imp.find_module(b, last_path)
        last_path = [path]
    if fp is None and desc[-1] == imp.PKG_DIRECTORY:
        return os.path.join(path, "__init__.py")
    return path


def package_script(filename, output, modules):
    module_sources = []
    for m in modules:
        path = find_module_path(m)
        print m, path
        source = open(path).read().encode("zlib").encode("base64")
        module_sources.append((m, source))
    packaged_script = open(output, "w")
    packaged_script.write("""#!/usr/bin/env python
### Compressed module sources ###
module_sources = %s
""" % repr(module_sources))

    packaged_script.write("""
### Load the compressed module sources ###
import sys, imp
for name, source in module_sources:
    source = source.decode("base64").decode("zlib")
    mod = imp.new_module(name)
    exec source in mod.__dict__
    sys.modules[name] = mod

### Original script follows ###
""")

    packaged_script.write(open(filename).read())

    packaged_script.close()

if __name__ == '__main__':
    from optparse import OptionParser

    parser = OptionParser(__doc__)
    parser.set_defaults(
        modules=[],
    )
    parser.add_option("-m", "--module", dest="modules", action="append", help="add extra module (e.g. if it's imported at runtime)")

    options, args = parser.parse_args()

    if len(args) != 2:
        parser.error("require a single script to package")

    package_script(args[0], args[1], options.modules)
