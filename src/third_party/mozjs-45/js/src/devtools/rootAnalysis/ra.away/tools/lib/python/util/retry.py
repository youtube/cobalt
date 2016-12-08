import os.path
import site

site.addsitedir(os.path.join(os.path.dirname(__file__), ".."))

from redo import retry, retriable, retrying, retrier

assert retry
assert retriable
assert retrying
assert retrier
