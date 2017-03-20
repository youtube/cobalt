import cPickle
import os
import re
import math
import time


def format_hist(h, units=1):
    retval = []
    if not h:
        h[0] = 0
    total = sum(h.values())
    keys = sorted(h.keys())
    min_key = min(keys)
    max_key = max(keys)

    for i in range(min_key, max_key + 1):
        n = h.get(i, 0)
        if total > 0:
            percentage = " %8.2f%%" % (n * 100. / total)
        else:
            percentage = ''

        retval.append("%3i: %8i%s"
                      % (i * units, n, percentage))
    return "\n".join(retval)


def scan_builder(builder, starttime, endtime, minutes_per_block, times, change_as_submittime=True):
    """Scans the build pickle files in the builder directory, and updates the dictionary `times`."""
    if not os.path.exists(builder):
        return
    for f in os.listdir(builder):
        if re.match("^\d+$", f):
            try:
                b = cPickle.load(open('%s/%s' % (builder, f)))
            except:
                continue

            if 'rebuild' in b.reason:
                # Skip rebuilds, they mess up the wait times
                continue

            if change_as_submittime:
                if len(b.changes) == 0:
                    continue
                submittime = b.changes[0].when
            else:
                submittime = b.requests[0].submittedAt

            if starttime < submittime < endtime:
                w = int(math.floor(
                    (b.started - submittime) / (minutes_per_block * 60.0)))
                times[w] = times.get(w, 0) + 1

if __name__ == "__main__":
    from optparse import OptionParser
    parser = OptionParser()
    parser.set_defaults(
        minutes_per_block=15,
        change_as_submittime=True,
        name=os.uname()[1],
        builders={},
        directory=None,
        starttime=time.time() - 24 * 3600,
        endtime=time.time(),
    )

    def add_builder(option, opt_str, value, parser, *args, **kwargs):
        if ":" in value:
            platform, builders = value.split(":", 1)
            builders = [b.strip() for b in builders.split(",")]
        else:
            platform = value
            builders = [platform]

        if platform not in parser.values.builders:
            parser.values.builders[platform] = []

        parser.values.builders[platform].extend(builders)

    parser.add_option("-m", "--minutes-per-block", type="int", help="How many minutes per block", dest="minutes_per_block")
    parser.add_option("-r", "--request-as-submittime",
                      action="store_false", dest="change_as_submittime")
    parser.add_option("-n", "--name", dest="name")
    parser.add_option("-b", "--builders", dest="builders", action="callback", nargs=1, type="string", callback=add_builder, help="platform:builder1,builder2,...")
    parser.add_option("-d", "--directory", dest="directory")
    parser.add_option("-s", "--start-time", dest="starttime", type="int")
    parser.add_option("-e", "--end-time", dest="endtime", type="int")
    parser.add_option("-a", "--address", dest="addresses", action="append")
    parser.add_option("-S", "--smtp", dest="smtp")
    parser.add_option("-f", "--from", dest="sender")

    options, args = parser.parse_args()

    if not options.builders:
        parser.error("Must specify some builders")

    if options.directory:
        os.chdir(options.directory)

    text = []
    text.append("Wait time report for %s for jobs submitted since %s\n"
                % (options.name, time.ctime(options.starttime)))

    hist = {}
    for platform, builders in options.builders.items():
        hist[platform] = {}
        for builder in builders:
            scan_builder(builder, options.starttime, options.endtime,
                         options.minutes_per_block, hist[platform],
                         options.change_as_submittime)

    allhist = {}
    for i in set([x for y in hist.keys() for x in hist[y]]):
        allhist[i] = sum([p.get(i, 0) for p in hist.values()])
    total = sum(allhist.values())
    text.append("Total Jobs: %i\n" % total)
    text.append("Wait Times")
    text.append(format_hist(allhist, options.minutes_per_block))
    text.append("\nPlatform break down\n")
    for platform, builders in options.builders.items():
        text.append("%s: %i" % (platform, sum(hist[platform].values())))
        text.append(format_hist(hist[platform], options.minutes_per_block))
        text.append("\n")

    text.append("The number on the left is how many minutes a build waited to start, rounded down")

    if not options.addresses:
        print "\n".join(text)
    else:
        import smtplib
        import sys

        zero_wait = 0
        if total > 0:
            zero_wait = allhist.get(0, 0) * 100. / total

        subject = "Wait: %i/%.2f%% (%s)" % (total, zero_wait, options.name)
        server = options.smtp or 'localhost'
        sender = options.sender or 'cltbld@build.mozilla.com'

        headers = []
        headers.append("Subject: %s" % subject)
        headers.append("From: %s" % sender)
        headers.append("To: %s" % (", ".join(options.addresses)))
        message = "\n".join(headers) + "\n" + "\n".join(text)

        try:
            smtp = smtplib.SMTP(server)
            smtp.sendmail(sender, options.addresses, message)
        except SMTPException:
            print "Error: unable to send email"
            sys.exit(1)
