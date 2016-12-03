from util.commands import run_cmd, get_output

import logging
log = logging.getLogger(__name__)


def downloadNightlyBuild(localeSrcDir, env):
    run_cmd(["make", "wget-en-US"], cwd=localeSrcDir, env=env)
    run_cmd(["make", "unpack"], cwd=localeSrcDir, env=env)
    output = get_output(["make", "ident"], cwd=localeSrcDir, env=env)
    info = {}
    for line in output.splitlines():
        key, value = line.rstrip().split()
        info[key] = value
    return info
