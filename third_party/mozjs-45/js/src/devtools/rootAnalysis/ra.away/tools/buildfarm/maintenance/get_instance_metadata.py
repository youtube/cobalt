#!/usr/bin/env python
"teeny helper script to find and output instance metadata"
import sys
import os

if sys.platform not in ('win32', 'cygwin'):
    path = "/etc/instance_metadata.json"
else:
    path = r"C:\instance_metadata.json"

if os.path.exists(path) and os.access(path, os.R_OK):
    print open(path).read()
