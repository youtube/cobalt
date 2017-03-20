import json
import os
import re
import subprocess

from sixgill import Body
from collections import defaultdict, namedtuple

scriptdir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

HazardSummary = namedtuple('HazardSummary', ['function', 'variable', 'type', 'GCFunction', 'location'])

def equal(got, expected):
    if got != expected:
        print("Got '%s', expected '%s'" % (got, expected))

def extract_unmangled(func):
    return func.split('$')[-1]

class Test(object):
    def __init__(self, indir, outdir, cfg):
        self.indir = indir
        self.outdir = outdir
        self.cfg = cfg

    def infile(self, path):
        return os.path.join(self.indir, path)

    def binpath(self, prog):
        return os.path.join(self.cfg.sixgill_bin, prog)

    def compile(self, source):
        cmd = "{CXX} -c {source} -O3 -std=c++11 -fplugin={sixgill} -fplugin-arg-xgill-mangle=1".format(
            source=self.infile(source),
            CXX=self.cfg.cxx, sixgill=self.cfg.sixgill_plugin)
        if self.cfg.verbose:
            print("Running %s" % cmd)
        subprocess.check_call(["sh", "-c", cmd])

    def load_db_entry(self, dbname, pattern):
        if not isinstance(pattern, basestring):
            output = subprocess.check_output([self.binpath("xdbkeys"), dbname + ".xdb"])
            entries = output.splitlines()
            matches = [f for f in entries if re.search(pattern, f)]
            if len(matches) == 0:
                raise Exception("entry not found")
            if len(matches) > 1:
                raise Exception("multiple entries found")
            pattern = matches[0]

        output = subprocess.check_output([self.binpath("xdbfind"), "-json", dbname + ".xdb", pattern])
        return json.loads(output)

    def run_analysis_script(self, phase, upto=None):
        file("defaults.py", "w").write('''\
analysis_scriptdir = '{scriptdir}'
sixgill_bin = '{bindir}'
'''.format(scriptdir=scriptdir, bindir=self.cfg.sixgill_bin))
        cmd = [ os.path.join(scriptdir, "analyze.py"), phase ]
        if upto:
            cmd += [ "--upto", upto ]
        cmd.append("--source=%s" % self.indir)
        cmd.append("--objdir=%s" % self.outdir)
        cmd.append("--js=%s" % self.cfg.js)
        if self.cfg.verbose:
            cmd.append("--verbose")
            print("Running " + " ".join(cmd))
        subprocess.check_call(cmd)

    def computeGCTypes(self):
        self.run_analysis_script("gcTypes", upto="gcTypes")

    def computeHazards(self):
        self.run_analysis_script("callgraph")

    def load_text_file(self, filename, key=lambda l: l):
        return [processed for processed in (
            key(line.strip()) for line in file(os.path.join(self.outdir, filename))
        ) if processed is not None]

    def load_suppressed_functions(self):
        return set(self.load_text_file("suppressedFunctions.lst"))

    def load_gcTypes(self):
        def grab_type(line):
            m = re.match(r'^(GC\w+): (.*)', line)
            if m:
                return (m.group(1) + 's', m.group(2))

        gctypes = defaultdict(list)
        for collection, typename in self.load_text_file('gcTypes.txt', key=grab_type):
            gctypes[collection].append(typename)
        return gctypes

    def load_gcFunctions(self):
        return self.load_text_file('gcFunctions.lst', key=extract_unmangled)

    def load_hazards(self):
        def grab_hazard(line):
            m = re.match(r"Function '(.*?)' has unrooted '(.*?)' of type '(.*?)' live across GC call '(.*?)' at (.*)", line)
            if m:
                info = list(m.groups())
                info[0] = info[0].split("$")[-1]
                info[3] = info[3].split("$")[-1]
                return HazardSummary(*info)

        return self.load_text_file('rootingHazards.txt', key=grab_hazard)

    def process_body(self, body):
        return Body(body)

    def process_bodies(self, bodies):
        return [ self.process_body(b) for b in bodies ]
