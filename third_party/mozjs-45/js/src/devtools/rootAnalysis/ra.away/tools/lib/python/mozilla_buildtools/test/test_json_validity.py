import json
import unittest
from os import path

class JsonValidityTest(unittest.TestCase):

    def _loadJsonFile(self, jsonFilePath):
        try:
            with open(path.join(path.dirname(__file__), '..', '..', '..', '..', jsonFilePath)) as json_data:
                json.load(json_data)
        except ValueError as e:
            self.fail("JSON file invalid: '%s' - %s" % (jsonFilePath, e))
        except IOError as e:
            self.fail("Required json file missing from tools repository: '%s'" % jsonFilePath)
        except BaseException as e:
            self.fail("Exception occurred while checking validity of json file '%s': %s" % (jsonFilePath, e))

    def testDevicesJson(self):
        self._loadJsonFile('buildfarm/mobile/devices.json')

    def testProductionMasters(self):
        self._loadJsonFile('buildfarm/maintenance/production-masters.json')

    def testProductionBranches(self):
        self._loadJsonFile('buildfarm/maintenance/production-branches.json')

if __name__ == '__main__':
    unittest.main()
