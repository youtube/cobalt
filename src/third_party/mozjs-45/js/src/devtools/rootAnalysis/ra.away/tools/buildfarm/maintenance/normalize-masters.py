#!/usr/bin/python
import simplejson
import sys
import os

fn = sys.argv[1]
data = simplejson.load(open(sys.argv[1]))

data.sort(key=lambda master: master['name'])

os.rename(fn, '%s.bak' % fn)

open(fn, 'w').write(simplejson.dumps(data, indent=2, sort_keys=True))
