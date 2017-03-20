import collections
from copy import copy


class ChunkingError(Exception):
    pass


def getChunk(things, chunks, thisChunk):
    if thisChunk > chunks:
        raise ChunkingError("thisChunk (%d) is greater than total chunks (%d)" %
                           (thisChunk, chunks))
    possibleThings = copy(things)
    nThings = len(possibleThings)
    for c in range(1, chunks + 1):
        n = nThings / chunks
        # If our things aren't evenly divisible by the number of chunks
        # we need to append one more onto some of them
        if c <= (nThings % chunks):
            n += 1
        if c == thisChunk:
            return possibleThings[0:n]
        del possibleThings[0:n]


# From https://stackoverflow.com/questions/3232943/update-value-of-a-nested-dictionary-of-varying-depth
def recursive_update(d, u):
    for k, v in u.iteritems():
        if isinstance(v, collections.Mapping):
            r = recursive_update(d.get(k, {}), v)
            d[k] = r
        else:
            d[k] = u[k]
    return d
