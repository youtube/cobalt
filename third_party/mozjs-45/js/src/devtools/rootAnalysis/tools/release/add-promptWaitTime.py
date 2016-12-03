#! /usr/bin/env python

import os
import sys
from optparse import OptionParser

parser = OptionParser()
parser.add_option("-p", "--path", action="store", dest="basepath",
                  help="Path to process")
parser.add_option("-w", "--wait", action="store", type="int", dest="wait",
                  help="Path to process")
(options, args) = parser.parse_args()

if not options.basepath or not os.path.isdir(options.basepath):
    print "Error: path must be specified"
    sys.exit(1)

if not options.wait or options.wait < 0:
    print "Error: wait time must be an integer greater than 0"
    sys.exit(1)

mod_count = 0
total_count = 0

for root, dirs, files in os.walk(options.basepath):
    for f in files:
        fullpath = os.path.join(root, f)
        total_count += 1

        # leave empty files alone
        if os.path.getsize(fullpath) == 0:
            continue

        content = open(fullpath).readlines()

        # bail if this is version=1 snippet
        if 'version=1' in content[0]:
            continue

        new_content = [c for c in content if 'promptWaitTime' not in c]
        new_content.append('promptWaitTime=%i' % options.wait)

        if new_content != content:
            fd = open(fullpath,'w')
            fd.writelines(new_content)
            fd.close
            mod_count += 1

print "modified %s of %s files" % (mod_count, total_count)
