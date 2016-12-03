import os
import os.path
import subprocess
import zipfile
import zlib

SAVED_ENV = {}

basePath = '/home/talos/public_html/'

dirtyProfiles = ['places_generated_med/places.sqlite',
                 'places_generated_min/places.sqlite']

maxDirtyProfile = ['places_generated_max/places.sqlite']

updateCmd = ['python',
             '/home/talos/generator/places/builddb/increment_dates.py']

profilesList = [['dirtyDBs.zip', dirtyProfiles],
                ['dirtyMaxDBs.zip', maxDirtyProfile]]


def setEnvironmentVar(key, val):
    global SAVED_ENV
    env = os.environ
    if key in env:
        SAVED_ENV[key] = env[key]
    else:
        SAVED_ENV[key] = ''
    env[key] = val


def restoreEnvironment():
    global SAVED_ENV
    for var in SAVED_ENV:
        os.environ[var] = SAVED_ENV[var]
    SAVED_ENV = {}


def runCmd(cmd):
    subprocess.call(cmd)


def removeZip():
    os.remove(zipName)


def createZip(zipName):
    zip = zipfile.ZipFile(basePath + zipName, 'w')
    for val in profiles:
        zip.write(basePath + val, val, compress_type=zipfile.ZIP_DEFLATED)
    zip.close()


def updateProfiles(profiles):
    key = 'PLACES_DB_PATH'
    for val in profiles:
        setEnvironmentVar('PYTHONPATH', '$PYTHONPATH:/usr/local/bin')
        setEnvironmentVar('PYTHONPATH', '/home/talos/generator')
        setEnvironmentVar(key, basePath + val)
        print "\nUpdating " + val + "\n"
        runCmd(updateCmd)
        restoreEnvironment()


if __name__ == '__main__':
    for name, profiles in profilesList:
        updateProfiles(profiles)
        createZip(name)
