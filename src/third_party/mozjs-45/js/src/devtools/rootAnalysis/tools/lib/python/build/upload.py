

def postUploadCmdPrefix(upload_dir=None,
                        branch=None,
                        product=None,
                        revision=None,
                        version=None,
                        who=None,
                        builddir=None,
                        buildid=None,
                        buildNumber=None,
                        to_tinderbox_dated=False,
                        to_tinderbox_builds=False,
                        to_dated=False,
                        to_latest=False,
                        to_try=False,
                        to_shadow=False,
                        to_candidates=False,
                        to_mobile_candidates=False,
                        nightly_dir=None,
                        signed=False,
                        ):
    """Returns a post_upload.py command line for the given arguments.
    It is expected that the returned value is augmented with the list of files
    to upload, and where to upload it.
    """

    cmd = ["post_upload.py"]

    if upload_dir:
        cmd.extend(["--tinderbox-builds-dir", upload_dir])
    if branch:
        cmd.extend(["-b", branch])
    if product:
        cmd.extend(["-p", product])
    if buildid:
        cmd.extend(['-i', buildid])
    if buildNumber:
        cmd.extend(['-n', str(buildNumber)])
    if version:
        cmd.extend(['-v', version])
    if revision:
        cmd.extend(['--revision', revision])
    if who:
        cmd.extend(['--who', who])
    if builddir:
        cmd.extend(['--builddir', builddir])
    if to_tinderbox_dated:
        cmd.append('--release-to-tinderbox-dated-builds')
    if to_tinderbox_builds:
        cmd.append('--release-to-tinderbox-builds')
    if to_try:
        cmd.append('--release-to-try-builds')
    if to_latest:
        cmd.append("--release-to-latest")
    if to_dated:
        cmd.append("--release-to-dated")
    if to_shadow:
        cmd.append("--release-to-shadow-central-builds")
    if to_candidates:
        cmd.append("--release-to-candidates-dir")
    if to_mobile_candidates:
        cmd.append("--release-to-mobile-candidates-dir")
    if nightly_dir:
        cmd.append("--nightly-dir=%s" % nightly_dir)
    if signed:
            cmd.append("--signed")

    return str(" ".join(cmd))
