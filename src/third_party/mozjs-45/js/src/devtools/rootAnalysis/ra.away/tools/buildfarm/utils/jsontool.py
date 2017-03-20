#!/usr/bin/python
"""Retrieve a key from a JSON file, possibly multiple levels deep"""
try:
    import json
except ImportError:
    import simplejson as json

if __name__ == '__main__':
    from optparse import OptionParser
    parser = OptionParser(usage=__doc__)
    parser.add_option("-k", "--key", dest="key",
                      help="Name of key to retrieve. If multiple levels deep, full path to the key, delimited by the specified delimiter.")
    parser.add_option("-d", "--delimiter", dest="delimiter",
                      help="Delimiter used in key when retrieving a value multiple levels deep")
    parser.set_defaults(delimiter=".")

    options, args = parser.parse_args()

    for f in args:
        js = json.load(open(f))
        if options.key:
            keys = options.key.split(options.delimiter)
            v = js
            for k in keys:
                v = v[k]
            print v
