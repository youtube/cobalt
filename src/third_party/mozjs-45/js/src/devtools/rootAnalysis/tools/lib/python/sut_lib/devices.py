import os
import json


def loadDevicesData(filepath):
    result = {}
    tFile = os.path.join(filepath, 'devices.json')
    if os.path.isfile(tFile):
        try:
            result = json.load(open(tFile, 'r'))
        except:
            result = {}
    return result

allDevices = loadDevicesData('/builds/tools/buildfarm/mobile')
if len(allDevices) == 0:
    allDevices = loadDevicesData(
        os.path.join(os.path.dirname(__file__), '../../../buildfarm/mobile'))
pandas = dict()
for x in allDevices:
    if x.startswith('panda-'):
        pandas[x] = allDevices[x]
