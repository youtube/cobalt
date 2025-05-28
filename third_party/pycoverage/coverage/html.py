"""HTML reporting for Coverage."""

import os, re, shutil, sys

import coverage
from coverage.backward import pickle
from coverage.misc import CoverageException, Hasher
from coverage.phystokens import source_token_lines, source_encoding
from coverage.report import Reporter
from coverage.results import Numbers
from coverage.templite import Templite


# Static files are looked for in a list of places.
STATIC_PATH = [
    # The place Debian puts system Javascript libraries.
    "/usr/share/javascript",

    # Our htmlfiles directory.
    os.path.join(os.path.dirname(__file__), "htmlfiles"),
]

def data_filename(fname, pkgdir=""):
    """Return the path to a data file of ours.

    The file is searched for on `STATIC_PATH`, and the first place it's found,
    is returned.

    Each directory in `STATIC_PATH` is searched as-is, and also, if `pkgdir`
    is provided, at that subdirectory.

    """
    for static_dir in STATIC_PATH:
        static_filename = os.path.join(static_dir, fname)
        if os.path.exists(static_filename):
            return static_filename
        if pkgdir:
            static_filename = os.path.join(static_dir, pkgdir, fname)
            if os.path.exists(static_filename):
                return static_filename
    raise CoverageException("Couldn't find static file %r" % fname)


def data(fname):
    """Return the contents of a data file of ours."""
    data_file = open(data_filename(fname))
    try:
        return data_file.read()
    finally:
        data_file.close()


class HtmlReporter(Reporter):
    """HTML reporting."""

    # These files will be copied from the htmlfiles dir to the output dir.
    STATIC_FILES = [
            ("style.css", ""),
            ("jquery.min.js", "jquery"),
            ("jquery.hotkeys.js", "jquery-hotkeys"),
            ("jquery.isonscreen.js", "jquery-isonscreen"),
            ("jquery.tablesorter.min.js", "jquery-tablesorter"),
            ("coverage_html.js", ""),
            ("keybd_closed.png", ""),
            ("keybd_open.png", ""),
            ]

    def __init__(self, cov, config):
        super(HtmlReporter, self).__init__(cov, config)
        self.directory = None
        self.template_globals = {
            'escape': escape,
            'title': self.config.html_title,
            '__url__': coverage.__url__,
            '__version__': coverage.__version__,
            }
        self.source_tmpl = Templite(
            data("pyfile.html"), self.template_globals
            )

        self.coverage = cov

        self.files = []
        self.arcs = self.coverage.data.has_arcs()
        self.status = HtmlStatus()
        self.extra_css = None
        self.totals = Numbers()

    def report(self, morfs):
        """Generate an HTML report for `morfs`.

        `morfs` is a list of modules or filenames.

        """
        assert self.config.html_dir, "must give a directory for html reporting"

        # Read the status data.
        self.status.read(self.config.html_dir)

        # Check that this run used the same settings as the last run.
        m = Hasher()
        m.update(self.config)
        these_settings = m.digest()
        if self.status.settings_hash() != these_settings:
            self.status.reset()
            self.status.set_settings_hash(these_settings)

        # The user may have extra CSS they want copied.
        if self.config.extra_css:
            self.extra_css = os.path.basename(self.config.extra_css)

        # Process all the files.
        self.report_files(self.html_file, morfs, self.config.html_dir)

        if not self.files:
            raise CoverageException("No data to report.")

        # Write the index file.
        self.index_file()

        self.make_local_static_report_files()

        return self.totals.pc_covered

    def make_local_static_report_files(self):
        """Make local instances of static files for HTML report."""
        # The files we provide must always be copied.
        for static, pkgdir in self.STATIC_FILES:
            shutil.copyfile(
                data_filename(static, pkgdir),
                os.path.join(self.directory, static)
                )

        # The user may have extra CSS they want copied.
        if self.extra_css:
            shutil.copyfile(
                self.config.extra_css,
                os.path.join(self.directory, self.extra_css)
                )

    def write_html(self, fname, html):
        """Write `html` to `fname`, properly encoded."""
        fout = open(fname, "wb")
        try:
            fout.write(html.encode('ascii', 'xmlcharrefreplace'))
        finally:
            fout.close()

    def file_hash(self, source, cu):
        """Compute a hash that changes if the file needs to be re-reported."""
        m = Hasher()
        m.update(source)
        self.coverage.data.add_to_hash(cu.filename, m)
        return m.digest()

    def html_file(self, cu, analysis):
        """Generate an HTML file for one source file."""
        source_file = cu.source_file()
        try:
            source = source_file.read()
        finally:
            source_file.close()

        # Find out if the file on disk is already correct.
        flat_rootname = cu.flat_rootname()
        this_hash = self.file_hash(source, cu)
        that_hash = self.status.file_hash(flat_rootname)
        if this_hash == that_hash:
            # Nothing has changed to require the file to be reported again.
            self.files.append(self.status.index_info(flat_rootname))
            return

        self.status.set_file_hash(flat_rootname, this_hash)

        # If need be, determine the encoding of the source file. We use it
        # later to properly write the HTML.
        if sys.version_info < (3, 0):
            encoding = source_encoding(source)
            # Some UTF8 files have the dreaded UTF8 BOM. If so, junk it.
            if encoding.startswith("utf-8") and source[:3] == "\xef\xbb\xbf":
                source = source[3:]
                encoding = "utf-8"

        # Get the numbers for this file.
        nums = analysis.numbers

        if self.arcs:
            missing_branch_arcs = analysis.missing_branch_arcs()

        # These classes determine which lines are highlighted by default.
        c_run = "run hide_run"
        c_exc = "exc"
        c_mis = "mis"
        c_par = "par " + c_run

        lines = []

        for lineno, line in enumerate(source_token_lines(source)):
            lineno += 1     # 1-based line numbers.
            # Figure out how to mark this line.
            line_class = []
            annotate_html = ""
            annotate_title = ""
            if lineno in analysis.statements:
                line_class.append("stm")
            if lineno in analysis.excluded:
                line_class.append(c_exc)
            elif lineno in analysis.missing:
                line_class.append(c_mis)
            elif self.arcs and lineno in missing_branch_arcs:
                line_class.append(c_par)
                annlines = []
                for b in missing_branch_arcs[lineno]:
                    if b < 0:
                        annlines.append("exit")
                    else:
                        annlines.append(str(b))
                annotate_html = "&nbsp;&nbsp; ".join(annlines)
                if len(annlines) > 1:
                    annotate_title = "no jumps to these line numbers"
                elif len(annlines) == 1:
                    annotate_title = "no jump to this line number"
            elif lineno in analysis.statements:
                line_class.append(c_run)

            # Build the HTML for the line
            html = []
            for tok_type, tok_text in line:
                if tok_type == "ws":
                    html.append(escape(tok_text))
                else:
                    tok_html = escape(tok_text) or '&nbsp;'
                    html.append(
                        "<span class='%s'>%s</span>" % (tok_type, tok_html)
                        )

            lines.append({
                'html': ''.join(html),
                'number': lineno,
                'class': ' '.join(line_class) or "pln",
                'annotate': annotate_html,
                'annotate_title': annotate_title,
            })

        # Write the HTML page for this file.
        html = spaceless(self.source_tmpl.render({
            'c_exc': c_exc, 'c_mis': c_mis, 'c_par': c_par, 'c_run': c_run,
            'arcs': self.arcs, 'extra_css': self.extra_css,
            'cu': cu, 'nums': nums, 'lines': lines,
        }))

        if sys.version_info < (3, 0):
            html = html.decode(encoding)

        html_filename = flat_rootname + ".html"
        html_path = os.path.join(self.directory, html_filename)
        self.write_html(html_path, html)

        # Save this file's information for the index file.
        index_info = {
            'nums': nums,
            'html_filename': html_filename,
            'name': cu.name,
            }
        self.files.append(index_info)
        self.status.set_index_info(flat_rootname, index_info)

    def index_file(self):
        """Write the index.html file for this report."""
        index_tmpl = Templite(
            data("index.html"), self.template_globals
            )

        self.totals = sum([f['nums'] for f in self.files])

        html = index_tmpl.render({
            'arcs': self.arcs,
            'extra_css': self.extra_css,
            'files': self.files,
            'totals': self.totals,
        })

        if sys.version_info < (3, 0):
            html = html.decode("utf-8")
        self.write_html(
            os.path.join(self.directory, "index.html"),
            html
            )

        # Write the latest hashes for next time.
        self.status.write(self.directory)


class HtmlStatus(object):
    """The status information we keep to support incremental reporting."""

    STATUS_FILE = "status.dat"
    STATUS_FORMAT = 1

    def __init__(self):
        self.reset()

    def reset(self):
        """Initialize to empty."""
        self.settings = ''
        self.files = {}

    def read(self, directory):
        """Read the last status in `directory`."""
        usable = False
        try:
            status_file = os.path.join(directory, self.STATUS_FILE)
            fstatus = open(status_file, "rb")
            try:
                status = pickle.load(fstatus)
            finally:
                fstatus.close()
        except (IOError, ValueError):
            usable = False
        else:
            usable = True
            if status['format'] != self.STATUS_FORMAT:
                usable = False
            elif status['version'] != coverage.__version__:
                usable = False

        if usable:
            self.files = status['files']
            self.settings = status['settings']
        else:
            self.reset()

    def write(self, directory):
        """Write the current status to `directory`."""
        status_file = os.path.join(directory, self.STATUS_FILE)
        status = {
            'format': self.STATUS_FORMAT,
            'version': coverage.__version__,
            'settings': self.settings,
            'files': self.files,
            }
        fout = open(status_file, "wb")
        try:
            pickle.dump(status, fout)
        finally:
            fout.close()

    def settings_hash(self):
        """Get the hash of the coverage.py settings."""
        return self.settings

    def set_settings_hash(self, settings):
        """Set the hash of the coverage.py settings."""
        self.settings = settings

    def file_hash(self, fname):
        """Get the hash of `fname`'s contents."""
        return self.files.get(fname, {}).get('hash', '')

    def set_file_hash(self, fname, val):
        """Set the hash of `fname`'s contents."""
        self.files.setdefault(fname, {})['hash'] = val

    def index_info(self, fname):
        """Get the information for index.html for `fname`."""
        return self.files.get(fname, {}).get('index', {})

    def set_index_info(self, fname, info):
        """Set the information for index.html for `fname`."""
        self.files.setdefault(fname, {})['index'] = info


# Helpers for templates and generating HTML

def escape(t):
    """HTML-escape the text in `t`."""
    return (t
            # Convert HTML special chars into HTML entities.
            .replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
            .replace("'", "&#39;").replace('"', "&quot;")
            # Convert runs of spaces: "......" -> "&nbsp;.&nbsp;.&nbsp;."
            .replace("  ", "&nbsp; ")
            # To deal with odd-length runs, convert the final pair of spaces
            # so that "....." -> "&nbsp;.&nbsp;&nbsp;."
            .replace("  ", "&nbsp; ")
        )

def spaceless(html):
    """Squeeze out some annoying extra space from an HTML string.

    Nicely-formatted templates mean lots of extra space in the result.
    Get rid of some.

    """
    html = re.sub(r">\s+<p ", ">\n<p ", html)
    return html
