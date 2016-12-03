import sys
from slavealloc.scripts import disable


def setup_argparse(subparsers):
    subparser = subparsers.add_parser(
        'enable', help='enable a slave, allowing it to start')
    subparser.add_argument('slave', help="slave to enable")
    subparser.add_argument('-m', '--message',
                           dest='message', help="new slave notes")
    return subparser


def process_args(subparser, args):
    if not args.slave:
        subparser.error("slave name is required")
    if '.' in ''.join(args.slave):
        subparser.error(
            "slave name must not contain '.'; give the unqualified hostname")


def main(args):
    if args.message is None:
        print >>sys.stderr, "NOTE: not clearing slave notes"
    return disable.set_enabled(args, args.slave, True, args.message)
