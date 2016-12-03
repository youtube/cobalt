class NoAllocationError(Exception):
    "base class for errors that should result in a 404"


class CmdlineError(Exception):
    "A message for the command-line user"
