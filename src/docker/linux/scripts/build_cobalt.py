#!/usr/bin/env python

import os
import subprocess

subprocess.check_output(["/code/cobalt/build/gyp_cobalt",
                         "-v",
                         "-C",
                         os.environ.get('CONFIG', 'devel'),
                         os.environ.get('TARGET', 'linux-x64x11')])
outdir = "out/" + os.environ.get('TARGET') + "_" + os.environ.get('CONFIG')
print("OUTDIR is {}".format(outdir))
subprocess.check_output(["ninja", "-C", "%s" % outdir, "cobalt"])
