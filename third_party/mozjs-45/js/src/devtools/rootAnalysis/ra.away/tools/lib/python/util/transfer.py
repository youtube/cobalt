from util.commands import run_cmd


def scp(src, dst, sshKey=None):
    cmd = ['scp', '-o', 'BatchMode=yes']
    if sshKey:
        cmd.extend(['-o', 'IdentityFile=%s' % sshKey])
    cmd.extend([src, dst])
    return run_cmd(cmd)
