import unittest

from util.algorithms import ChunkingError, getChunk


class TestGetChunk(unittest.TestCase):
    def setUp(self):
        self.data = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]

    def testOneChunk(self):
        self.assertEquals(getChunk(self.data, 1, 1), self.data)

    def testNGreaterThanTotal(self):
        self.assertRaises(ChunkingError, getChunk, self.data, 1, 2)

    def testMultipleChunks(self):
        self.assertEquals(getChunk(self.data, 5, 3), [5, 6])

    def testNotEvenlyDivisibleWithExtra(self):
        self.assertEquals(getChunk(self.data, 4, 2), [4, 5, 6])

    def testNotEvenlyDivisibleWithoutExtra(self):
        self.assertEquals(getChunk(self.data, 4, 3), [7, 8])
