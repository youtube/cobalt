#!/usr/bin/python
import sys
import os

import site
site.addsitedir(os.path.join(os.path.dirname(os.path.realpath(__file__)), "../lib/python"))

from sut_lib.relay import connected_socket, powercycle, set_status, get_status
from sut_lib.relay import PORT

if __name__ == '__main__':
    # Basic commandline interface for testing.
    def usage():
        print "Usage: %s [powercycle|status|turnon|turnoff] <hostname> <bank> <relay>" % sys.argv[0]
        sys.exit(1)
    if len(sys.argv) != 5:
        usage()
    cmd, hostname, bank, relay = sys.argv[1:5]
    bank, relay = int(bank), int(relay)
    if cmd == 'powercycle':
        if powercycle(hostname, bank, relay):
            print "OK"
        else:
            print "FAILED"
    elif cmd == 'status':
        with connected_socket(hostname, PORT) as sock:
            print "bank %d, relay %d status: %d" % (bank, relay, get_status(sock, bank, relay))
    elif cmd == 'turnon' or cmd == 'turnoff':
        with connected_socket(hostname, PORT) as sock:
            status = cmd == 'turnon'
            if status == set_status(sock, bank, relay, status):
                print "OK"
            else:
                print "FAILED"
    else:
        usage()
    sys.exit(0)
