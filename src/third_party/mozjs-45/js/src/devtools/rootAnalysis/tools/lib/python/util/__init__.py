import hashlib
import binascii


def sha1string(s):
    "Return the sha1 hash of the string s"
    return hashlib.sha1(s).digest()


def b64(s):
    "Return s base64 encoded, with tailing whitespace and = removed"
    return binascii.b2a_base64(s).rstrip("=\n")


def b64sha1sum(s):
    "Returns the base64 encoded version of the sha1sum of s"
    return b64(sha1string(s))
