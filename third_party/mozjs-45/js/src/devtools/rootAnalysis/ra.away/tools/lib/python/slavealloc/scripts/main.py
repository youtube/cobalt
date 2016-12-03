import sys
import argparse
from twisted.internet import defer, reactor
from twisted.python import log
from slavealloc import exceptions

# subcommands
from slavealloc.scripts import dbinit, gettac, lock, disable, enable, dbdump, dbimport, notes
subcommands = [dbinit, gettac, lock, disable, enable, dbdump, dbimport, notes]


def parse_options():
    parser = argparse.ArgumentParser(description="Runs slavealloc subcommands")
    parser.set_defaults(_module=None)

    parser.add_argument('-A', '--api', dest='apiurl',
                        default='http://slavealloc.build.mozilla.org/api',
                        help="""URL of the REST API to use for most subcommands""")

    subparsers = parser.add_subparsers(title='subcommands')

    for module in subcommands:
        subparser = module.setup_argparse(subparsers)
        subparser.set_defaults(module=module, subparser=subparser)

    # parse the args
    args = parser.parse_args()

    # make sure we got a subcommand
    if not args.module:
        parser.error("No subcommand specified")

    # let it process its own args
    args.module.process_args(args.subparser, args)

    # and return the results
    return args.module.main, args


def main():
    errors = []

    func, args = parse_options()

    def do_command():
        d = defer.maybeDeferred(func, args)

        # catch command-line errors and don't show the traceback
        def cmdline_error(f):
            f.trap(exceptions.CmdlineError)
            errors.append(str(f.value))
        d.addErrback(cmdline_error)

        # but catch everything else..
        d.addErrback(log.err, "while executing subcommand")

        # before unconditionally stopping the reactor
        d.addBoth(lambda _: reactor.stop())
    reactor.callWhenRunning(do_command)
    reactor.run()

    # handle any errors after the reactor is done
    if errors:
        for error in errors:
            print >>sys.stderr, error
        sys.exit(1)
